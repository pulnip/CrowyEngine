#include <array>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "FramePipeline.hpp"
#include "PostChain.hpp"

using namespace Crowy;

namespace
{
    using Posts = std::vector<PostPassDesc>;
    using Reads = std::vector<FrameTargetID>;

    constexpr auto BackBufferFormat = RHIPixelFormat::RGBA8_UNORM;
    constexpr u32 Width = 64;
    constexpr u32 Height = 32;

    constexpr FrameTargetID SceneDepth = 1;
    constexpr FrameTargetID SceneColor = 2;
    constexpr FrameTargetID Normals = 3;
    // the first intermediate a chain adds
    constexpr FrameTargetID FirstAdded = 4;

    // one pass writing scene colour and normals over depth; the chain
    // supplies the back buffer's writer
    FramePipelineDesc SceneShaped() {
        return FramePipelineDesc{
            .targets =
                {FrameTargetDesc{
                     .name = "SceneDepth",
                     .format = RHIPixelFormat::D32_FLOAT
                 },
                 FrameTargetDesc{
                     .name = "SceneColor",
                     .format = RHIPixelFormat::RGBA16_FLOAT
                 },
                 FrameTargetDesc{
                     .name = "Normals",
                     .format = RHIPixelFormat::RGBA8_UNORM
                 }},
            .passes = {PassDesc{
                .name = "Opaque",
                .colors =
                    {ColorTargetUse{.target = SceneColor},
                     ColorTargetUse{.target = Normals}},
                .depth = DepthTargetUse{.target = SceneDepth},
                .kind = MeshPassDesc{}
            }},
            .sceneColor = SceneColor
        };
    }

    PostPassDesc Entry(
        CStr name,
        PostOutput output,
        std::vector<Str> inputs = {}
    ) {
        return PostPassDesc{
            .name = name,
            .fragmentShader =
                {.path = "Engine/Render/Test/Post.slang", .entryPoint = "fs_main"},
            .output = output,
            .inputs = std::move(inputs)
        };
    }

    const FullscreenPassDesc& Fullscreen(const PassDesc& pass) {
        return std::get<FullscreenPassDesc>(pass.kind);
    }

    // the walker takes what the chain made: every read follows its write
    void ExpectWalkable(FramePipelineDesc desc) {
        FakeDevice device;
        try {
            FramePipeline pipeline(
                device,
                std::move(desc),
                BackBufferFormat,
                Width,
                Height
            );
            EXPECT_EQ(
                pipeline.Overlay(),
                (OverlayFormats{
                    .color = BackBufferFormat,
                    .depth = RHIPixelFormat::Unknown
                })
            );
        } catch(const std::invalid_argument& e) {
            ADD_FAILURE() << "the walker refused the chain: " << e.what();
        }
    }

    void ExpectRefused(
        const Posts& post,
        StrView expected,
        FrameTargetID sceneColor = SceneColor
    ) {
        auto desc = SceneShaped();
        try {
            appendPostChain(desc, sceneColor, post);
            ADD_FAILURE() << "accepted a list that should say: " << expected;
        } catch(const std::invalid_argument& e) {
            EXPECT_TRUE(StrView{e.what()}.contains(expected))
                << e.what() << "\nexpected: " << expected;
        }
        // a refused list adds nothing
        EXPECT_EQ(desc.targets.size(), 3u) << expected;
        EXPECT_EQ(desc.passes.size(), 1u) << expected;
    }
}

TEST(PostChain, TonemapAloneNeedsNoIntermediate) {
    auto desc = SceneShaped();
    const std::array post{tonemapPass()};

    appendPostChain(desc, SceneColor, post);

    EXPECT_EQ(desc.targets.size(), 3u);
    ASSERT_EQ(desc.passes.size(), 2u);
    const auto& tonemap = desc.passes[1];
    EXPECT_EQ(tonemap.name, "Tonemap");
    ASSERT_EQ(tonemap.colors.size(), 1u);
    EXPECT_EQ(tonemap.colors[0].target, BackBufferTarget);
    EXPECT_EQ(tonemap.colors[0].load, RHILoadAction::Clear);
    EXPECT_EQ(tonemap.colors[0].store, RHIStoreAction::Store);
    EXPECT_FALSE(tonemap.depth.has_value());
    EXPECT_EQ(tonemap.reads, Reads{SceneColor});
    EXPECT_EQ(
        Fullscreen(tonemap).fragmentShader,
        (RHIShaderDesc{
            .path = "Engine/Render/Shader/Tonemap.slang",
            .entryPoint = "fs_main"
        })
    );
    EXPECT_FALSE(Fullscreen(tonemap).blend.has_value());

    // the other engine entry ends a chain the same way
    const auto present = presentPass();
    EXPECT_EQ(present.name, "Present");
    EXPECT_EQ(present.output, PostOutput::Display);
    EXPECT_EQ(
        present.fragmentShader,
        (RHIShaderDesc{
            .path = "Engine/Render/Shader/Present.slang",
            .entryPoint = "fs_main"
        })
    );

    ExpectWalkable(std::move(desc));
}

TEST(PostChain, TonemapThenADisplayEntryShareOneRgba8Intermediate) {
    constexpr FrameTargetID PostDisplay0 = FirstAdded;

    auto desc = SceneShaped();
    const std::array post{
        tonemapPass(),
        Entry("Posterize", PostOutput::Display)
    };

    appendPostChain(desc, SceneColor, post);

    ASSERT_EQ(desc.targets.size(), 4u);
    const auto& intermediate = desc.targets[PostDisplay0 - 1];
    EXPECT_EQ(intermediate.name, "PostDisplay0");
    EXPECT_EQ(intermediate.format, RHIPixelFormat::RGBA8_UNORM);
    // swapchain-sized, so a Resize recreates it
    EXPECT_EQ(intermediate.width, 0u);
    EXPECT_EQ(intermediate.height, 0u);

    ASSERT_EQ(desc.passes.size(), 3u);
    EXPECT_EQ(desc.passes[1].name, "Tonemap");
    EXPECT_EQ(desc.passes[1].colors[0].target, PostDisplay0);
    EXPECT_EQ(desc.passes[1].reads, Reads{SceneColor});
    EXPECT_EQ(desc.passes[2].name, "Posterize");
    EXPECT_EQ(desc.passes[2].colors[0].target, BackBufferTarget);
    EXPECT_EQ(desc.passes[2].reads, Reads{PostDisplay0});

    ExpectWalkable(std::move(desc));
}

TEST(PostChain, ThreeSceneEntriesPingPong) {
    constexpr FrameTargetID PostScene0 = FirstAdded;
    constexpr FrameTargetID PostScene1 = FirstAdded + 1;

    auto desc = SceneShaped();
    const std::array post{
        Entry("Bloom", PostOutput::Scene),
        Entry("Fog", PostOutput::Scene),
        Entry("Grade", PostOutput::Scene),
        tonemapPass()
    };

    appendPostChain(desc, SceneColor, post);

    // two in scene colour's format, and none of the display class
    ASSERT_EQ(desc.targets.size(), 5u);
    EXPECT_EQ(desc.targets[PostScene0 - 1].name, "PostScene0");
    EXPECT_EQ(desc.targets[PostScene1 - 1].name, "PostScene1");
    for(const auto id: {PostScene0, PostScene1}) {
        EXPECT_EQ(desc.targets[id - 1].format, RHIPixelFormat::RGBA16_FLOAT)
            << id;
    }

    ASSERT_EQ(desc.passes.size(), 5u);
    const std::array<std::pair<FrameTargetID, FrameTargetID>, 4> expected{
        std::pair{SceneColor, PostScene0},
        std::pair{PostScene0, PostScene1},
        std::pair{PostScene1, PostScene0},
        std::pair{PostScene0, BackBufferTarget}
    };
    for(usize i = 0; i < expected.size(); ++i) {
        const auto& pass = desc.passes[i + 1];
        const auto [source, output] = expected[i];
        EXPECT_EQ(pass.name, post[i].name);
        EXPECT_EQ(pass.reads, Reads{source}) << pass.name;
        ASSERT_EQ(pass.colors.size(), 1u) << pass.name;
        EXPECT_EQ(pass.colors[0].target, output) << pass.name;
    }

    ExpectWalkable(std::move(desc));
}

TEST(PostChain, InputsBindAfterTheSource) {
    constexpr Vec4 Params{0.05f, 0.4f, 0.05f, 100.0f};

    auto desc = SceneShaped();
    auto outline =
        Entry("Outline", PostOutput::Scene, {"SceneDepth", "Normals"});
    outline.params = Params;
    const std::array post{outline, tonemapPass()};

    appendPostChain(desc, SceneColor, post);

    ASSERT_EQ(desc.passes.size(), 3u);
    const auto& pass = desc.passes[1];
    EXPECT_EQ(pass.reads, (Reads{SceneColor, SceneDepth, Normals}));
    EXPECT_EQ(Fullscreen(pass).params, Params);
    EXPECT_EQ(Fullscreen(pass).fragmentShader, outline.fragmentShader);
    // the inputs ride only the entry that names them
    EXPECT_EQ(desc.passes[2].reads, Reads{FirstAdded});

    ExpectWalkable(std::move(desc));
}

TEST(PostChain, InvalidListsAreRefused) {
    ExpectRefused({}, "the post list is empty");
    ExpectRefused(
        {Entry("Grade", PostOutput::Scene)},
        "post entry 'Grade': it is last and outputs Scene"
    );
    ExpectRefused(
        {tonemapPass(), Entry("Bloom", PostOutput::Scene), presentPass()},
        "post entry 'Bloom': it outputs Scene after 'Tonemap' output Display"
    );
    ExpectRefused(
        {Entry("Outline", PostOutput::Display, {"SceneDepth", "Velocity"})},
        "post entry 'Outline': input 'Velocity' is not a target; the targets "
        "are 'SceneDepth', 'SceneColor', 'Normals'"
    );
    ExpectRefused(
        {Entry(
            "Wide",
            PostOutput::Display,
            {"SceneDepth", "Normals", "SceneDepth", "Normals"}
        )},
        "post entry 'Wide': it names 4 inputs; a post entry reads at most 3"
    );
    ExpectRefused(
        {tonemapPass()},
        "the post chain reads scene colour from target 0, which is not a "
        "target",
        BackBufferTarget
    );
    ExpectRefused(
        {tonemapPass()},
        "the post chain reads scene colour from target 9",
        9
    );
}
