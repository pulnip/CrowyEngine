#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "EnumUtil.hpp"
#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "FramePipeline.hpp"
#include "LinearAlgebra.hpp"
#include "RHICommandList.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"
#include "StandardPipeline.hpp"

using namespace Crowy;

namespace
{
    using Objects = std::vector<u32>;
    using Pushes = std::vector<ScenePush>;
    using RecordedPass = FakeCommandList::RecordedPass;
    using TextureBarriers = std::vector<RHITextureBarrier>;

    constexpr auto BackBufferFormat = RHIPixelFormat::RGBA8_UNORM;
    constexpr u32 Width = 64;
    constexpr u32 Height = 32;
    constexpr u64 Vertices = 0xFE;

    // a fragment entry per pipeline, so two entries key two pipelines
    MaterialPipelineDesc Pipeline(
        CStr fragmentEntry,
        MaterialDomain domain = MaterialDomain::Opaque
    ) {
        return MaterialPipelineDesc{
            .vertexShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "vs_main"},
            .fragmentShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = fragmentEntry},
            .maskShader = {.path = "Engine/Shader/X.slang", .entryPoint = "fs_masked_depth"},
            .domain = domain,
            .profile = "sm_6_8"
        };
    }

    MeshPassDesc& Mesh(PassDesc& pass) {
        return std::get<MeshPassDesc>(pass.kind);
    }

    const MeshPassDesc& Mesh(const PassDesc& pass) {
        return std::get<MeshPassDesc>(pass.kind);
    }

    // the standard list with a small map, its Shadow pass biased: a
    // fixed-size depth-only pass on view 1 drawing casters only
    FramePipelineDesc ShadowShaped(const PassDepthBias& bias) {
        auto desc = makeStandardPipeline({.shadowMapSize = 64});
        Mesh(desc.passes[0]).state.depthBias = bias;

        return desc;
    }

    const RHIShaderDesc compositeShader{
        .path = "Engine/Render/Test/Composite.slang",
        .entryPoint = "fs_composite"
    };
    constexpr Vec4 CompositeParams{1.0f, 2.0f, 3.0f, 4.0f};

    // a mesh pass into an offscreen color and depth, which one fullscreen
    // pass reads onto the back buffer
    FramePipelineDesc CompositeShaped() {
        constexpr FrameTargetID SceneDepth = 1;
        constexpr FrameTargetID SceneColor = 2;

        return FramePipelineDesc{
            .targets =
                {FrameTargetDesc{
                     .name = "SceneDepth",
                     .format = RHIPixelFormat::D32_FLOAT
                 },
                 FrameTargetDesc{
                     .name = "SceneColor",
                     .format = RHIPixelFormat::RGBA16_FLOAT
                 }},
            .passes =
                {PassDesc{
                     .name = "Opaque",
                     .colors = {ColorTargetUse{.target = SceneColor}},
                     .depth = DepthTargetUse{.target = SceneDepth},
                     .kind = MeshPassDesc{}
                 },
                 PassDesc{
                     .name = "Composite",
                     .colors = {ColorTargetUse{.target = BackBufferTarget}},
                     .reads = {SceneColor, SceneDepth},
                     .kind =
                         FullscreenPassDesc{
                             .fragmentShader = compositeShader,
                             .params = CompositeParams
                         }
                 }},
            .sceneColor = SceneColor
        };
    }

    // a scene whose primitives all sit inside both views' clip space
    class Fixture {
    public:
        FakeDevice device;
        RenderScene scene;
        SceneRenderer renderer{device, SceneRendererDesc{.viewCount = 2}};
        FakeCommandList cmdList;
        FakeTexture backBuffer{BackBufferFormat, Width, Height, 0xBB};
        FakeBuffer indexBuffer{64};

        MaterialHandle AddMaterial(
            CStr fragmentEntry,
            MaterialDomain domain = MaterialDomain::Opaque
        ) {
            return scene.Materials().Add(
                MaterialResource{.pipeline = Pipeline(fragmentEntry, domain)}
            );
        }

        // returns the primitive's row
        u32 AddPrimitive(
            MaterialHandle material,
            PrimitiveFlags flags =
                combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
        ) {
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes = {SubMesh{.geometry = {.indexCount = 3}}},
                    .materials = {material}
                }
            );
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .worldBounds =
                        AABB3D{
                            .center = {0.0f, 0.0f, 0.5f},
                            .halfScale = 0.1f * ones()
                        },
                    .mesh = mesh,
                    .flags = flags
                }
            );

            return static_cast<u32>(scene.Primitives().Count() - 1);
        }

        FrameInputs Inputs() {
            return FrameInputs{
                .backBuffer = &backBuffer,
                .indices = RHIIndexBufferView{.buffer = &indexBuffer},
                .vertices = Vertices
            };
        }

        void Prepare(
            FramePipeline& pipeline,
            const MeshPassOverride& debug = {}
        ) {
            renderer.BeginFrame(scene);
            pipeline.Prepare(renderer, scene, debug);
            renderer.Upload();
        }

        void Frame(FramePipeline& pipeline, FrameInputs& inputs) {
            Prepare(pipeline);
            cmdList.Begin();
            pipeline.Record(cmdList, renderer, inputs);
            cmdList.Close();
        }

        const RecordedPass& Pass(StrView name) const {
            for(const auto& pass: cmdList.passes) {
                if(pass.event == name)
                    return pass;
            }
            ADD_FAILURE() << "no pass named " << name;
            static const RecordedPass none;

            return none;
        }

        // the rows a pass's draws read, as its push names them
        Objects DrawnObjects(const RecordedPass& pass) const {
            if(pass.pushes.empty())
                return {};

            ScenePush push;
            std::memcpy(&push, pass.pushes.back().data(), sizeof(push));
            u32 draws = 0;
            for(const auto& batch: pass.batches)
                draws += batch.drawCount;

            Objects objects;
            for(u32 i = 0; i < draws; ++i) {
                objects.push_back(device.transient
                                      .Read<DrawData>(
                                          (push.drawBase + i) *
                                          static_cast<u32>(sizeof(DrawData))
                                      )
                                      .objectID);
            }

            return objects;
        }
    };

    void ExpectRefused(FramePipelineDesc desc, StrView expected) {
        FakeDevice device;
        try {
            FramePipeline pipeline(
                device,
                std::move(desc),
                BackBufferFormat,
                Width,
                Height
            );
            ADD_FAILURE() << "accepted a desc that should say: " << expected;
        } catch(const std::invalid_argument& e) {
            EXPECT_TRUE(StrView{e.what()}.contains(expected))
                << e.what() << "\nexpected: " << expected;
        }
    }
}

TEST(FramePipeline, StandardPipelineIsPrepassOpaqueTranslucent) {
    constexpr FrameTargetID SceneColor = 2;

    const auto desc = makeStandardPipeline({.shadowMapSize = 0});

    ASSERT_EQ(desc.targets.size(), 2u);
    EXPECT_EQ(desc.targets[0].name, "SceneDepth");
    EXPECT_EQ(desc.targets[0].format, RHIPixelFormat::D32_FLOAT);
    EXPECT_EQ(desc.targets[0].width, 0u);
    EXPECT_EQ(desc.sceneColor, SceneColor);
    // the scene passes, then the post list's one entry
    ASSERT_EQ(desc.passes.size(), 4u);

    const auto& prepass = desc.passes[0];
    EXPECT_EQ(prepass.name, "DepthPrepass");
    EXPECT_TRUE(prepass.colors.empty());
    ASSERT_TRUE(prepass.depth.has_value());
    EXPECT_EQ(prepass.depth->load, RHILoadAction::Clear);
    EXPECT_EQ(prepass.depth->store, RHIStoreAction::Store);
    EXPECT_EQ(Mesh(prepass).filter.domains, combine(MaterialDomain::Opaque, MaterialDomain::Masked));
    EXPECT_EQ(Mesh(prepass).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(Mesh(prepass).state.depthWrite);

    const auto& opaque = desc.passes[1];
    EXPECT_EQ(opaque.name, "Opaque");
    ASSERT_EQ(opaque.colors.size(), 1u);
    EXPECT_EQ(opaque.colors[0].target, SceneColor);
    EXPECT_EQ(opaque.colors[0].load, RHILoadAction::Clear);
    EXPECT_EQ(opaque.colors[0].store, RHIStoreAction::Store);
    ASSERT_TRUE(opaque.depth.has_value());
    EXPECT_EQ(opaque.depth->load, RHILoadAction::Load);
    EXPECT_EQ(opaque.depth->store, RHIStoreAction::Store);
    EXPECT_EQ(Mesh(opaque).filter.domains, combine(MaterialDomain::Opaque, MaterialDomain::Masked));
    EXPECT_EQ(Mesh(opaque).order, DrawOrder::PipelineThenNearFirst);
    EXPECT_EQ(Mesh(opaque).state.depthFunc, RHIComparisonFunc::Equal);
    EXPECT_FALSE(Mesh(opaque).state.depthWrite);

    const auto& translucent = desc.passes[2];
    EXPECT_EQ(translucent.name, "Translucent");
    ASSERT_EQ(translucent.colors.size(), 1u);
    EXPECT_EQ(translucent.colors[0].target, SceneColor);
    EXPECT_EQ(translucent.colors[0].load, RHILoadAction::Load);
    EXPECT_EQ(translucent.colors[0].store, RHIStoreAction::Store);
    ASSERT_TRUE(translucent.depth.has_value());
    EXPECT_EQ(translucent.depth->load, RHILoadAction::Load);
    EXPECT_EQ(translucent.depth->store, RHIStoreAction::DontCare);
    EXPECT_EQ(Mesh(translucent).filter.domains, MaterialDomain::Translucent);
    EXPECT_EQ(Mesh(translucent).order, DrawOrder::FarFirst);
    EXPECT_EQ(Mesh(translucent).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_FALSE(Mesh(translucent).state.depthWrite);

    EXPECT_EQ(desc.passes[3].name, "Tonemap");
}

TEST(FramePipeline, StandardPipelineWithoutPrepassWritesDepthInOpaque) {
    const auto desc = makeStandardPipeline(
        {.depthPrepass = false, .shadowMapSize = 0}
    );

    ASSERT_EQ(desc.passes.size(), 3u);
    const auto& opaque = desc.passes[0];
    EXPECT_EQ(opaque.name, "Opaque");
    ASSERT_TRUE(opaque.depth.has_value());
    EXPECT_EQ(opaque.depth->load, RHILoadAction::Clear);
    EXPECT_EQ(opaque.depth->store, RHIStoreAction::Store);
    EXPECT_EQ(Mesh(opaque).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(Mesh(opaque).state.depthWrite);
    EXPECT_EQ(desc.passes[1].name, "Translucent");
    EXPECT_EQ(desc.passes[2].name, "Tonemap");
}

TEST(FramePipeline, TheStandardDescOpensWithTheShadowPass) {
    constexpr FrameTargetID ShadowMap = 2;
    constexpr FrameTargetID SceneColor = 3;

    auto desc = makeStandardPipeline({});

    ASSERT_EQ(desc.targets.size(), 3u);
    EXPECT_EQ(desc.targets[0].name, "SceneDepth");
    EXPECT_EQ(desc.targets[1].name, "ShadowMap");
    EXPECT_EQ(desc.targets[2].name, "SceneColor");
    EXPECT_EQ(desc.targets[1].format, RHIPixelFormat::D32_FLOAT);
    EXPECT_EQ(desc.targets[1].width, 2048u);
    EXPECT_EQ(desc.targets[1].height, 2048u);
    EXPECT_EQ(desc.shadowMap, ShadowMap);
    ASSERT_EQ(desc.passes.size(), 5u);

    const auto& shadow = desc.passes[0];
    EXPECT_EQ(shadow.name, "Shadow");
    EXPECT_TRUE(shadow.colors.empty());
    ASSERT_TRUE(shadow.depth.has_value());
    EXPECT_EQ(shadow.depth->target, ShadowMap);
    EXPECT_EQ(shadow.depth->load, RHILoadAction::Clear);
    EXPECT_EQ(shadow.depth->store, RHIStoreAction::Store);
    EXPECT_TRUE(shadow.reads.empty());
    EXPECT_EQ(Mesh(shadow).view, SceneRenderer::ShadowView);
    EXPECT_EQ(Mesh(shadow).filter.domains, combine(MaterialDomain::Opaque, MaterialDomain::Masked));
    EXPECT_EQ(Mesh(shadow).filter.required, PrimitiveFlags::CastShadow);
    EXPECT_EQ(Mesh(shadow).order, DrawOrder::PipelineThenNearFirst);
    EXPECT_EQ(Mesh(shadow).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(Mesh(shadow).state.depthWrite);
    EXPECT_FALSE(Mesh(shadow).state.fragmentShader.has_value());
    EXPECT_FALSE(Mesh(shadow).state.depthBias.has_value());

    EXPECT_EQ(desc.passes[1].name, "DepthPrepass");
    EXPECT_TRUE(desc.passes[1].reads.empty());
    EXPECT_EQ(desc.passes[2].name, "Opaque");
    EXPECT_EQ(desc.passes[2].reads, std::vector<FrameTargetID>{ShadowMap});
    EXPECT_EQ(desc.passes[3].name, "Translucent");
    EXPECT_EQ(desc.passes[3].reads, std::vector<FrameTargetID>{ShadowMap});
    EXPECT_EQ(desc.passes[4].name, "Tonemap");
    EXPECT_EQ(desc.passes[4].reads, std::vector<FrameTargetID>{SceneColor});

    FakeDevice device;
    FramePipeline
        pipeline(device, std::move(desc), BackBufferFormat, Width, Height);
    EXPECT_EQ(pipeline.ViewCount(), 2u);
    EXPECT_EQ(pipeline.ShadowMapSize(), 2048u);
}

TEST(FramePipeline, AZeroShadowMapSizeDropsThePassAndTheTarget) {
    auto desc = makeStandardPipeline({.shadowMapSize = 0});

    ASSERT_EQ(desc.targets.size(), 2u);
    EXPECT_EQ(desc.shadowMap, 0u);
    ASSERT_EQ(desc.passes.size(), 4u);
    for(const auto& pass: desc.passes) {
        EXPECT_NE(pass.name, "Shadow");
        if(const auto* mesh = std::get_if<MeshPassDesc>(&pass.kind)) {
            EXPECT_TRUE(pass.reads.empty()) << pass.name;
            EXPECT_EQ(mesh->view, 0u) << pass.name;
        }
    }

    FakeDevice device;
    FramePipeline
        pipeline(device, std::move(desc), BackBufferFormat, Width, Height);
    EXPECT_EQ(pipeline.ViewCount(), 1u);
    EXPECT_EQ(pipeline.ShadowMapSize(), 0u);
    EXPECT_EQ(device.textureCreates.size(), 2u);
}

TEST(FramePipeline, TheStandardDescEndsInTonemapWithTheUI) {
    constexpr FrameTargetID SceneColor = 3;
    constexpr Color Sky{0.139f, 0.212f, 0.356f, 1.0f};

    auto desc = makeStandardPipeline({.clearColor = Sky});

    // swapchain-sized, after the map, cleared with the config's colour
    ASSERT_EQ(desc.targets.size(), 3u);
    const auto& color = desc.targets[SceneColor - 1];
    EXPECT_EQ(color.name, "SceneColor");
    EXPECT_EQ(color.format, RHIPixelFormat::RGBA16_FLOAT);
    EXPECT_EQ(color.width, 0u);
    EXPECT_EQ(color.height, 0u);
    EXPECT_EQ(color.clearColor, Sky);
    EXPECT_EQ(desc.sceneColor, SceneColor);

    ASSERT_EQ(desc.passes.size(), 5u);
    const auto& tonemap = desc.passes[4];
    EXPECT_EQ(tonemap.name, "Tonemap");
    ASSERT_EQ(tonemap.colors.size(), 1u);
    EXPECT_EQ(tonemap.colors[0].target, BackBufferTarget);
    EXPECT_EQ(tonemap.reads, std::vector<FrameTargetID>{SceneColor});
    EXPECT_EQ(
        std::get<FullscreenPassDesc>(tonemap.kind).fragmentShader,
        tonemapPass().fragmentShader
    );

    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline
        pipeline(f.device, std::move(desc), BackBufferFormat, Width, Height);
    // the UI rides the post pass, which has no depth
    EXPECT_EQ(
        pipeline.Overlay(),
        (OverlayFormats{
            .color = BackBufferFormat,
            .depth = RHIPixelFormat::Unknown
        })
    );
    auto inputs = f.Inputs();
    inputs.sceneClear = Sky;
    f.Frame(pipeline, inputs);

    // the scene colour is cleared with the colour it was created with
    ASSERT_EQ(f.device.textureCreates.size(), 3u);
    EXPECT_EQ(f.device.textureCreates[SceneColor - 1].clearColor, Sky);
    EXPECT_EQ(f.Pass("Opaque").colors[0].clearColor, Sky);
    EXPECT_EQ(f.Pass("Tonemap").colors[0].texture, &f.backBuffer);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, ADisplayReadyDescEndsInPresent) {
    constexpr FrameTargetID SceneColor = 2;

    const auto desc = makeStandardPipeline({
        .sceneColorFormat = RHIPixelFormat::RGBA8_UNORM,
        .clearColor = Colors::Magenta,
        .shadowMapSize = 0,
        .post = {presentPass()}
    });

    // one Display entry: no intermediate
    ASSERT_EQ(desc.targets.size(), 2u);
    EXPECT_EQ(
        desc.targets[SceneColor - 1].format,
        RHIPixelFormat::RGBA8_UNORM
    );
    EXPECT_EQ(desc.targets[SceneColor - 1].clearColor, Colors::Magenta);
    ASSERT_EQ(desc.passes.size(), 4u);
    const auto& present = desc.passes[3];
    EXPECT_EQ(present.name, "Present");
    ASSERT_EQ(present.colors.size(), 1u);
    EXPECT_EQ(present.colors[0].target, BackBufferTarget);
    EXPECT_EQ(present.reads, std::vector<FrameTargetID>{SceneColor});
    EXPECT_EQ(
        std::get<FullscreenPassDesc>(present.kind).fragmentShader,
        presentPass().fragmentShader
    );
}

// every half as the edge rule gives it, two frames running
TEST(FramePipeline, WalkRecordsEveryEdgeOfTheStandardPipeline) {
    using enum RHIResourceUsage;

    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    for(int frame = 0; frame < 2; ++frame) {
        f.Frame(pipeline, inputs);

        ASSERT_EQ(f.cmdList.passes.size(), 4u);
        auto& depth = *f.Pass("DepthPrepass").depth->texture;
        auto& color = *f.Pass("Opaque").colors[0].texture;
        auto& back = f.backBuffer;

        EXPECT_EQ(
            f.Pass("DepthPrepass").acquires,
            TextureBarriers{
                MakeCrossSubmissionBarrier(depth, DepthWrite, DepthWrite, true)
            }
        );
        EXPECT_EQ(
            f.Pass("DepthPrepass").releases,
            TextureBarriers{MakeBarrier(depth, DepthWrite, DepthWrite)}
        );
        EXPECT_EQ(
            f.Pass("Opaque").acquires,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, DepthWrite),
                MakeCrossSubmissionBarrier(
                    color,
                    SampledFragment,
                    RenderTarget,
                    true
                )
            })
        );
        EXPECT_EQ(
            f.Pass("Opaque").releases,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, DepthWrite),
                MakeBarrier(color, RenderTarget, RenderTarget)
            })
        );
        EXPECT_EQ(
            f.Pass("Translucent").acquires,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, DepthWrite),
                MakeBarrier(color, RenderTarget, RenderTarget)
            })
        );
        EXPECT_EQ(
            f.Pass("Translucent").releases,
            TextureBarriers{MakeBarrier(color, RenderTarget, SampledFragment)}
        );
        EXPECT_EQ(
            f.Pass("Tonemap").acquires,
            (TextureBarriers{
                MakeBarrier(back, Undefined, RenderTarget),
                MakeBarrier(color, RenderTarget, SampledFragment)
            })
        );
        EXPECT_EQ(
            f.Pass("Tonemap").releases,
            TextureBarriers{MakeBarrier(back, RenderTarget, Present)}
        );

        EXPECT_TRUE(f.cmdList.violations.empty())
            << f.cmdList.violations.front().what;
        EXPECT_TRUE(f.cmdList.unconsumedAtClose.empty());
#if CROWY_FRAME_STATS
        EXPECT_EQ(f.cmdList.GetStats().renderPassCount, 4u);
        EXPECT_EQ(f.cmdList.GetStats().barrierEdgeCount, 12u);
#endif
    }
}

TEST(FramePipeline, BackBufferIsAcquiredUndefinedAndReleasedToPresent) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();
    inputs.sceneClear = Color{0.25f, 0.5f, 0.75f, 1.0f};

    f.Frame(pipeline, inputs);

    // the scene clear lands on scene colour; the back buffer, which only the
    // post pass writes, clears black
    const auto& opaque = f.Pass("Opaque");
    ASSERT_EQ(opaque.colors.size(), 1u);
    EXPECT_NE(opaque.colors[0].texture, &f.backBuffer);
    EXPECT_EQ(opaque.colors[0].clearColor, inputs.sceneClear);
    EXPECT_EQ(f.Pass("Translucent").colors[0].loadAction, RHILoadAction::Load);

    const auto& tonemap = f.Pass("Tonemap");
    ASSERT_EQ(tonemap.colors.size(), 1u);
    EXPECT_EQ(tonemap.colors[0].texture, &f.backBuffer);
    EXPECT_EQ(tonemap.colors[0].loadAction, RHILoadAction::Clear);
    EXPECT_EQ(tonemap.colors[0].clearColor, Colors::Black);
    ASSERT_FALSE(tonemap.acquires.empty());
    EXPECT_TRUE(tonemap.acquires.front().discard);
    EXPECT_EQ(
        tonemap.acquires.front().layoutBefore,
        RHITextureLayout::Undefined
    );
    EXPECT_EQ(f.cmdList.LayoutOf(&f.backBuffer), RHITextureLayout::Present);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

// a fresh texture, the frame before's last use, and a texture a Resize made
TEST(FramePipeline, FirstUseAcquiresAcrossSubmissionsAndDiscards) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    const auto expectFirstUse = [&f] {
        const auto& acquire = f.Pass("DepthPrepass").acquires.front();
        EXPECT_TRUE(acquire.crossSubmission);
        EXPECT_TRUE(acquire.discard);
        EXPECT_EQ(acquire.layoutBefore, RHITextureLayout::Undefined);
        EXPECT_EQ(acquire.syncBefore, RHIBarrierSync::DepthStencil);
    };

    f.Frame(pipeline, inputs);
    expectFirstUse();
    auto* first = f.Pass("DepthPrepass").depth->texture;

    f.Frame(pipeline, inputs);
    expectFirstUse();
    EXPECT_EQ(f.Pass("DepthPrepass").depth->texture, first);
    EXPECT_EQ(f.cmdList.LayoutOf(first), RHITextureLayout::DepthWrite);

    pipeline.Resize(128, 64);
    f.Frame(pipeline, inputs);
    expectFirstUse();
    auto* resized = f.Pass("DepthPrepass").depth->texture;
    EXPECT_NE(resized, first);
    EXPECT_EQ(resized->GetWidth(), 128u);
    EXPECT_EQ(resized->GetHeight(), 64u);

    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, EveryMeshPassRebindsItsViewAndPush) {
    Fixture f;
    const auto opaque = f.AddPrimitive(f.AddMaterial("fs_opaque"));
    const auto glass =
        f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    Pushes pushes;
    auto inputs = f.Inputs();
    inputs.bindMeshPass =
        [&pushes](RHICommandList& cmdList, const ScenePush& push) {
            pushes.push_back(push);
            cmdList.SetPushGraphicsConstants(push);
        };

    f.Frame(pipeline, inputs);

    ASSERT_EQ(pushes.size(), 3u);
    for(const auto& push: pushes)
        EXPECT_EQ(push.vertices, Vertices);

    const auto viewOffset =
        f.Pass("DepthPrepass").constantBuffers.front().offset;
    for(const auto& pass: f.cmdList.passes) {
        ASSERT_EQ(pass.constantBuffers.size(), 1u) << pass.event;
        EXPECT_EQ(pass.constantBuffers[0].slot, ViewConstantBufferSlot);
        EXPECT_EQ(pass.constantBuffers[0].offset, viewOffset);
        EXPECT_EQ(pass.pushes.size(), 1u) << pass.event;
    }

    // each pass's push names its own list's rows
    EXPECT_EQ(f.DrawnObjects(f.Pass("DepthPrepass")), Objects{opaque});
    EXPECT_EQ(f.DrawnObjects(f.Pass("Opaque")), Objects{opaque});
    EXPECT_EQ(f.DrawnObjects(f.Pass("Translucent")), Objects{glass});
    EXPECT_NE(pushes[1].drawBase, pushes[2].drawBase);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

// the pass that writes the map, and one before it, push none
TEST(FramePipeline, APassThatReadsTheShadowMapGetsItsID) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        ShadowShaped(PassDepthBias{}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);

    const auto pushOf = [&f](StrView name) {
        const auto& pass = f.Pass(name);
        ScenePush push;
        EXPECT_EQ(pass.pushes.size(), 1u) << name;
        if(!pass.pushes.empty())
            std::memcpy(&push, pass.pushes[0].data(), sizeof(push));

        return push;
    };
    const auto map = f.Pass("Shadow").depth->texture->GetReadableID();
    EXPECT_NE(map, 0u);
    EXPECT_EQ(pushOf("Shadow").shadowMap, 0u);
    EXPECT_EQ(pushOf("DepthPrepass").shadowMap, 0u);
    EXPECT_EQ(pushOf("Opaque").shadowMap, map);
    EXPECT_EQ(pushOf("Translucent").shadowMap, map);

    // no mesh pass of a list with no map pushes one
    Fixture g;
    g.AddPrimitive(g.AddMaterial("fs_opaque"));
    FramePipeline plain(
        g.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    auto plainInputs = g.Inputs();
    g.Frame(plain, plainInputs);
    for(const auto* name: {"DepthPrepass", "Opaque", "Translucent"}) {
        const auto& pass = g.Pass(name);
        ScenePush push;
        ASSERT_EQ(pass.pushes.size(), 1u) << name;
        ASSERT_EQ(pass.pushes[0].size(), sizeof(push)) << name;
        std::memcpy(&push, pass.pushes[0].data(), sizeof(push));
        EXPECT_EQ(push.shadowMap, 0u) << name;
    }
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, GeometryAcquiresRideEveryMeshPass) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    FakeBuffer vertexBuffer{64};
    const std::array uploads{
        MakeBarrier(
            vertexBuffer,
            RHIResourceUsage::CopyDst,
            RHIResourceUsage::VertexBuffer
        ),
        MakeBarrier(
            f.indexBuffer,
            RHIResourceUsage::CopyDst,
            RHIResourceUsage::IndexBuffer
        )
    };
    auto inputs = f.Inputs();
    inputs.geometryAcquires = uploads;

    f.Prepare(pipeline);
    f.cmdList.Begin();
    // the pool's copies, recorded outside any pass
    f.cmdList.BeginBlitPass();
    f.cmdList.EndBlitPass({}, uploads);
    pipeline.Record(f.cmdList, f.renderer, inputs);
    f.cmdList.Close();

    // a fullscreen pass draws no geometry, so it carries none
    for(const auto& pass: f.cmdList.passes) {
        const auto expected =
            pass.event == "Tonemap"
                ? std::vector<RHIBufferBarrier>{}
                : std::vector<RHIBufferBarrier>{uploads.begin(), uploads.end()};
        EXPECT_EQ(pass.bufferAcquires, expected) << pass.event;
    }
    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_TRUE(f.cmdList.unconsumedBuffersAtClose.empty());

    const auto stats = pipeline.Stats();
    ASSERT_EQ(stats.size(), 4u);
    EXPECT_EQ(stats[0].barrierEdges, 2u + 2u);
    EXPECT_EQ(stats[1].barrierEdges, 4u + 2u);
    EXPECT_EQ(stats[2].barrierEdges, 3u + 2u);
    EXPECT_EQ(stats[3].barrierEdges, 3u);
}

TEST(FramePipeline, OverlayRidesTheLastBackBufferPass) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    FakeTexture atlas{RHIPixelFormat::RGBA8_UNORM, 8, 8, 0xA7};
    const std::array uiAcquires{MakeBarrier(
        atlas,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::SampledFragment
    )};
    auto inputs = f.Inputs();
    inputs.overlayAcquires = uiAcquires;
    inputs.recordOverlay = [](RHICommandList& cmdList) {
        cmdList.SetMarker("ui");
    };

    f.Frame(pipeline, inputs);

    EXPECT_EQ(
        pipeline.Overlay(),
        (OverlayFormats{
            .color = BackBufferFormat,
            .depth = RHIPixelFormat::Unknown
        })
    );
    for(const auto& pass: f.cmdList.passes) {
        const bool carries =
            std::ranges::contains(pass.acquires, uiAcquires[0]);
        const bool marked = std::ranges::contains(pass.log, Str{"marker ui"});
        EXPECT_EQ(carries, pass.event == "Tonemap") << pass.event;
        EXPECT_EQ(marked, pass.event == "Tonemap") << pass.event;
    }

    // after the pass's own draw
    const auto& log = f.Pass("Tonemap").log;
    ASSERT_GE(log.size(), 2u);
    EXPECT_EQ(log.back(), "marker ui");
    EXPECT_EQ(log[log.size() - 2], "draw");
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, ResizeRecreatesOnlySwapchainSizedTargets) {
    Fixture f;
    FramePipeline pipeline(
        f.device,
        ShadowShaped(PassDepthBias{}),
        BackBufferFormat,
        Width,
        Height
    );

    ASSERT_EQ(f.device.textureCreates.size(), 3u);
    EXPECT_EQ(f.device.textureCreates[0].width, Width);
    EXPECT_EQ(f.device.textureCreates[0].height, Height);
    EXPECT_EQ(f.device.textureCreates[0].usage, RHITextureUsage::DepthStencil);
    EXPECT_EQ(f.device.textureCreates[1].width, 64u);
    EXPECT_EQ(f.device.textureCreates[1].height, 64u);
    EXPECT_EQ(f.device.textureCreates[2].width, Width);
    EXPECT_EQ(f.device.textureCreates[2].height, Height);

    pipeline.Resize(128, 96);

    // the depth and the scene colour, not the map
    ASSERT_EQ(f.device.textureCreates.size(), 5u);
    for(usize i = 3; i < 5; ++i) {
        EXPECT_EQ(f.device.textureCreates[i].width, 128u) << i;
        EXPECT_EQ(f.device.textureCreates[i].height, 96u) << i;
    }
    EXPECT_EQ(f.device.textureCreates[3].format, RHIPixelFormat::D32_FLOAT);
    EXPECT_EQ(
        f.device.textureCreates[4].format,
        RHIPixelFormat::RGBA16_FLOAT
    );
    // a frame in flight may still read the old ones
    EXPECT_EQ(f.device.deferred.size(), 2u);
    EXPECT_EQ(f.device.texturesDestroyed, 0u);

    f.device.RunDeferred();
    EXPECT_EQ(f.device.texturesDestroyed, 2u);
}

TEST(FramePipeline, AShadowShapedPassDrawsCastersFromViewOne) {
    Fixture f;
    const auto material = f.AddMaterial("fs_opaque");
    const auto caster = f.AddPrimitive(material);
    const auto receiver = f.AddPrimitive(material, PrimitiveFlags::Visible);
    const PassDepthBias bias{.depthBias = 2, .slopeScaledDepthBias = 1.5f};
    FramePipeline
        pipeline(f.device, ShadowShaped(bias), BackBufferFormat, Width, Height);
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);

    const auto& shadow = f.Pass("Shadow");
    EXPECT_TRUE(shadow.colors.empty());
    ASSERT_TRUE(shadow.viewport.has_value());
    EXPECT_EQ(shadow.viewport->width, 64.0f);
    EXPECT_EQ(shadow.viewport->height, 64.0f);
    ASSERT_EQ(shadow.constantBuffers.size(), 1u);
    EXPECT_EQ(
        shadow.constantBuffers[0].offset,
        f.Pass("Opaque").constantBuffers[0].offset + sizeof(ViewData)
    );
    EXPECT_EQ(f.DrawnObjects(shadow), Objects{caster});
    EXPECT_EQ(f.DrawnObjects(f.Pass("Opaque")), (Objects{caster, receiver}));

    // the bias is in the key: the prepass's depth-only pipeline is another one
    ASSERT_EQ(shadow.batches.size(), 1u);
    auto* biased = &f.renderer.Pipelines().Resolve(
        f.scene.Materials().GetRef(material).pipeline,
        PassPipelineDesc{.state = MeshPassState{.depthBias = bias}}
    );
    EXPECT_EQ(shadow.batches[0].pso, biased);
    EXPECT_NE(shadow.batches[0].pso, f.Pass("DepthPrepass").batches[0].pso);

    auto* map = shadow.depth->texture;
    pipeline.Resize(128, 96);
    f.Frame(pipeline, inputs);
    EXPECT_EQ(f.Pass("Shadow").depth->texture, map);
    EXPECT_EQ(map->GetWidth(), 64u);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, APassNotInTheListCostsNothing) {
    const auto frameWith = [](bool depthPrepass) {
        Fixture f;
        f.AddPrimitive(f.AddMaterial("fs_opaque"));
        f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
        FramePipeline pipeline(
            f.device,
            makeStandardPipeline(
                {.depthPrepass = depthPrepass, .shadowMapSize = 0}
            ),
            BackBufferFormat,
            Width,
            Height
        );
        auto inputs = f.Inputs();
        f.Frame(pipeline, inputs);

        EXPECT_TRUE(f.cmdList.violations.empty());
        EXPECT_EQ(f.device.textureCreates.size(), 2u);

        return std::pair{f.cmdList.passes.size(), f.renderer.PipelineCount()};
    };

    // a depth-only key beside the two colour keys and the tone map's
    EXPECT_EQ(frameWith(true), (std::pair<usize, usize>{4, 4}));
    EXPECT_EQ(frameWith(false), (std::pair<usize, usize>{3, 3}));
}

// a Masked primitive draws in every pass that writes depth, its cut sharing
// one depth pipeline between the shadow map and the prepass
TEST(FramePipeline, AMaskedMaterialCutsInEveryPassThatWritesDepth) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_masked", MaterialDomain::Masked));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(f.device, makeStandardPipeline({.shadowMapSize = 64}), BackBufferFormat, Width, Height);
    auto inputs = f.Inputs();
    f.Frame(pipeline, inputs);

    EXPECT_TRUE(f.cmdList.violations.empty());
    // depth: opaque and masked; color: opaque, masked, glass; the tone map
    EXPECT_EQ(f.renderer.PipelineCount(), 6u);
    const auto cuts = std::ranges::count_if(f.device.pipelineCreates, [](const RHIGraphicsPipelineStateDesc& desc) {
        return desc.fragmentShader && desc.fragmentShader->entryPoint == "fs_masked_depth" && desc.renderTargetCount == 0;
    });
    EXPECT_EQ(cuts, 1);
}

TEST(FramePipeline, DestroyedPipelineRetiresItsTargets) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    {
        FramePipeline pipeline(
            f.device,
            ShadowShaped(PassDepthBias{}),
            BackBufferFormat,
            Width,
            Height
        );
        auto inputs = f.Inputs();
        f.Frame(pipeline, inputs);
    }

    EXPECT_EQ(f.device.deferred.size(), 3u);
    EXPECT_EQ(f.device.texturesDestroyed, 0u);
    f.device.RunDeferred();
    EXPECT_EQ(f.device.texturesDestroyed, 3u);
}

TEST(FramePipeline, PassStatsCountEachPass) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_a"));
    f.AddPrimitive(f.AddMaterial("fs_b"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);

    const auto stats = pipeline.Stats();
    ASSERT_EQ(stats.size(), 4u);
    // the two opaque materials share one depth-only pipeline
    EXPECT_EQ(stats[0].name, "DepthPrepass");
    EXPECT_EQ(stats[0].draws, 2u);
    EXPECT_EQ(stats[0].runs, 1u);
    EXPECT_EQ(stats[0].triangles, 2u);
    EXPECT_EQ(stats[0].barrierEdges, 2u);
    EXPECT_EQ(stats[1].name, "Opaque");
    EXPECT_EQ(stats[1].draws, 2u);
    EXPECT_EQ(stats[1].runs, 2u);
    EXPECT_EQ(stats[1].triangles, 2u);
    EXPECT_EQ(stats[1].barrierEdges, 4u);
    EXPECT_EQ(stats[2].name, "Translucent");
    EXPECT_EQ(stats[2].draws, 1u);
    EXPECT_EQ(stats[2].runs, 1u);
    EXPECT_EQ(stats[2].triangles, 1u);
    EXPECT_EQ(stats[2].barrierEdges, 3u);
    // a fullscreen pass has no list
    EXPECT_EQ(stats[3].name, "Tonemap");
    EXPECT_EQ(stats[3].draws, 0u);
    EXPECT_EQ(stats[3].runs, 0u);
    EXPECT_EQ(stats[3].triangles, 0u);
    EXPECT_EQ(stats[3].barrierEdges, 3u);
}

TEST(FramePipeline, InvalidDescsAreRefused) {
    const auto standard = [] {
        return makeStandardPipeline({.shadowMapSize = 0});
    };

    {
        auto desc = standard();
        desc.sceneColor = 5;
        ExpectRefused(std::move(desc), "sceneColor names target 5");
    }
    {
        auto desc = standard();
        desc.passes[1].colors[0].target = 9;
        ExpectRefused(std::move(desc), "pass 'Opaque': target 9 is unknown");
    }
    {
        auto desc = standard();
        desc.targets.push_back(FrameTargetDesc{.name = "SceneDepth"});
        ExpectRefused(std::move(desc), "two targets are named 'SceneDepth'");
    }
    {
        auto desc = standard();
        desc.passes[2].name = "Opaque";
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': two passes have this name"
        );
    }
    {
        auto desc = standard();
        desc.passes[1].colors.push_back(
            ColorTargetUse{.target = desc.sceneColor}
        );
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': it attaches target 2 twice"
        );
    }
    {
        auto desc = standard();
        desc.passes[0].depth->target = BackBufferTarget;
        ExpectRefused(
            std::move(desc),
            "pass 'DepthPrepass': the back buffer is not a depth target"
        );
    }
    {
        auto desc = standard();
        desc.passes[0].depth->load = RHILoadAction::Load;
        ExpectRefused(
            std::move(desc),
            "pass 'DepthPrepass': its first use of 'SceneDepth' keeps contents "
            "nothing wrote"
        );
    }
    {
        auto desc = standard();
        desc.passes[1].colors[0].load = RHILoadAction::Load;
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': its first use of 'SceneColor' keeps contents"
        );
    }
    {
        auto desc = standard();
        desc.passes[1].depth->store = RHIStoreAction::DontCare;
        ExpectRefused(
            std::move(desc),
            "pass 'Translucent': it keeps 'SceneDepth', which pass 'Opaque' "
            "stored DontCare"
        );
    }
    {
        auto desc = standard();
        desc.passes.resize(1);
        ExpectRefused(std::move(desc), "no pass writes the back buffer");
    }
    {
        auto desc = standard();
        desc.passes[1].depth.reset();
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': a mesh pass needs a depth target"
        );
    }
    {
        auto desc = standard();
        Mesh(desc.passes[0]).state.fragmentShader = RHIShaderDesc{
            .path = "Engine/Shader/X.slang",
            .entryPoint = "fs_normals"
        };
        ExpectRefused(
            std::move(desc),
            "pass 'DepthPrepass': a depth-only mesh pass has no fragment stage"
        );
    }
    {
        auto desc = standard();
        desc.targets.push_back(
            FrameTargetDesc{
                .name = "Extra",
                .format = RHIPixelFormat::RGBA8_UNORM
            }
        );
        desc.passes[3].colors.push_back(
            ColorTargetUse{
                .target = static_cast<FrameTargetID>(desc.targets.size())
            }
        );
        ExpectRefused(
            std::move(desc),
            "pass 'Tonemap': the overlay pass needs exactly one color target"
        );
    }
    {
        auto desc = standard();
        desc.shadowMap = 5;
        ExpectRefused(
            std::move(desc),
            "shadowMap names target 5, which is unknown"
        );
    }
    {
        // SceneDepth follows the swapchain's size
        auto desc = standard();
        desc.shadowMap = 1;
        ExpectRefused(
            std::move(desc),
            "shadowMap names target 1, which is not a fixed-size square target"
        );
    }
    {
        auto desc = standard();
        desc.targets.push_back(
            FrameTargetDesc{
                .name = "Oblong",
                .format = RHIPixelFormat::D32_FLOAT,
                .width = 64,
                .height = 32
            }
        );
        desc.shadowMap = static_cast<FrameTargetID>(desc.targets.size());
        ExpectRefused(
            std::move(desc),
            "shadowMap names target 3, which is not a fixed-size square target"
        );
    }
}

// a mesh pass leaves color and depth, and one fullscreen pass reads both
TEST(FramePipeline, FullscreenPassPushesItsReadsInOrder) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline
        pipeline(f.device, CompositeShaped(), BackBufferFormat, Width, Height);
    auto inputs = f.Inputs();
    inputs.sceneClear = Color{0.25f, 0.5f, 0.75f, 1.0f};

    f.Frame(pipeline, inputs);

    const auto& opaque = f.Pass("Opaque");
    const auto& composite = f.Pass("Composite");
    ASSERT_EQ(composite.pushes.size(), 1u);
    ASSERT_EQ(composite.pushes[0].size(), sizeof(FullscreenPush));
    FullscreenPush push;
    std::memcpy(&push, composite.pushes[0].data(), sizeof(push));
    EXPECT_EQ(push.source, opaque.colors[0].texture->GetReadableID());
    EXPECT_EQ(push.input0, opaque.depth->texture->GetReadableID());
    EXPECT_EQ(push.input1, 0u);
    EXPECT_EQ(push.input2, 0u);
    EXPECT_EQ(push.params, CompositeParams);

    // one triangle, with the view bound as every pass binds it
    EXPECT_EQ(composite.directDraws, 1u);
    EXPECT_TRUE(composite.batches.empty());
    ASSERT_EQ(composite.constantBuffers.size(), 1u);
    EXPECT_EQ(composite.constantBuffers[0].slot, ViewConstantBufferSlot);
    EXPECT_EQ(pipeline.Stats()[1].draws, 0u);

    // sceneColor names the target a debug view clears
    EXPECT_EQ(opaque.colors[0].clearColor, inputs.sceneClear);

    // vs_main from the fragment shader's own file, cull none, no depth
    const auto& key = f.device.pipelineCreates.back();
    const auto& frontend = std::get<RHILegacyFrontendDesc>(key.preRasterizer);
    EXPECT_TRUE(frontend.vertexShader.path == compositeShader.path);
    EXPECT_EQ(frontend.vertexShader.entryPoint, "vs_main");
    EXPECT_TRUE(key.fragmentShader == compositeShader);
    EXPECT_EQ(key.rasterizer.cullMode, RHICullMode::None);
    EXPECT_FALSE(key.depthStencil.has_value());
    EXPECT_FALSE(key.blend.has_value());
    ASSERT_EQ(key.renderTargetCount, 1u);
    EXPECT_EQ(key.renderTargetFormats[0], BackBufferFormat);

    // a read adds shader-resource usage to what the target is attached as
    ASSERT_EQ(f.device.textureCreates.size(), 2u);
    EXPECT_EQ(
        f.device.textureCreates[0].usage,
        combine(RHITextureUsage::DepthStencil, RHITextureUsage::ShaderResource)
    );
    EXPECT_EQ(
        f.device.textureCreates[1].usage,
        combine(RHITextureUsage::RenderTarget, RHITextureUsage::ShaderResource)
    );
    EXPECT_TRUE(f.cmdList.violations.empty());
}

// read after read is an edge too: only a chain keeps the next writer behind
// every reader on Metal
TEST(FramePipeline, ReadsCompileIntoAChainOfEdges) {
    using enum RHIResourceUsage;

    const auto halvesOf = [](const TextureBarriers& barriers, RHITexture* t) {
        TextureBarriers mine;
        for(const auto& half: barriers) {
            if(half.texture == t)
                mine.push_back(half);
        }

        return mine;
    };

    {
        constexpr FrameTargetID Chained = 1;
        constexpr FrameTargetID Side = 2;
        const auto fullscreen = [](CStr entry) {
            return FullscreenPassDesc{
                .fragmentShader = {
                    .path = "Engine/Render/Test/Chain.slang",
                    .entryPoint = entry
                }
            };
        };

        Fixture f;
        FramePipeline pipeline(
            f.device,
            FramePipelineDesc{
                .targets =
                    {FrameTargetDesc{
                         .name = "Chained",
                         .format = RHIPixelFormat::RGBA8_UNORM
                     },
                     FrameTargetDesc{
                         .name = "Side",
                         .format = RHIPixelFormat::RGBA8_UNORM
                     }},
                .passes =
                    {PassDesc{
                         .name = "Write",
                         .colors = {ColorTargetUse{.target = Chained}},
                         .kind = fullscreen("fs_write")
                     },
                     PassDesc{
                         .name = "ReadA",
                         .colors = {ColorTargetUse{.target = Side}},
                         .reads = {Chained},
                         .kind = fullscreen("fs_read")
                     },
                     PassDesc{
                         .name = "ReadB",
                         .colors = {ColorTargetUse{.target = BackBufferTarget}},
                         .reads = {Chained},
                         .kind = fullscreen("fs_read")
                     },
                     PassDesc{
                         .name = "Rewrite",
                         .colors = {ColorTargetUse{
                             .target = Chained,
                             .load = RHILoadAction::Load
                         }},
                         .kind = fullscreen("fs_write")
                     }}
            },
            BackBufferFormat,
            Width,
            Height
        );
        auto inputs = f.Inputs();

        for(int frame = 0; frame < 2; ++frame) {
            f.Frame(pipeline, inputs);

            auto& t = *f.Pass("Write").colors[0].texture;
            EXPECT_EQ(
                halvesOf(f.Pass("Write").acquires, &t),
                TextureBarriers{MakeCrossSubmissionBarrier(
                    t,
                    RenderTarget,
                    RenderTarget,
                    true
                )}
            );
            EXPECT_EQ(
                halvesOf(f.Pass("Write").releases, &t),
                TextureBarriers{MakeBarrier(t, RenderTarget, SampledFragment)}
            );
            EXPECT_EQ(
                halvesOf(f.Pass("ReadA").acquires, &t),
                TextureBarriers{MakeBarrier(t, RenderTarget, SampledFragment)}
            );
            EXPECT_EQ(
                halvesOf(f.Pass("ReadA").releases, &t),
                TextureBarriers{
                    MakeBarrier(t, SampledFragment, SampledFragment)
                }
            );
            EXPECT_EQ(
                halvesOf(f.Pass("ReadB").acquires, &t),
                TextureBarriers{
                    MakeBarrier(t, SampledFragment, SampledFragment)
                }
            );
            EXPECT_EQ(
                halvesOf(f.Pass("ReadB").releases, &t),
                TextureBarriers{MakeBarrier(t, SampledFragment, RenderTarget)}
            );
            EXPECT_EQ(
                halvesOf(f.Pass("Rewrite").acquires, &t),
                TextureBarriers{MakeBarrier(t, SampledFragment, RenderTarget)}
            );
            EXPECT_TRUE(f.Pass("Rewrite").releases.empty());

            EXPECT_TRUE(f.cmdList.violations.empty())
                << f.cmdList.violations.front().what;
            EXPECT_TRUE(f.cmdList.unconsumedAtClose.empty());
        }
    }

    // the map's two readers, which the standard desc lists: a mesh pass's
    // reads compile into edges and usage, and the map alone reaches its push
    {
        Fixture f;
        f.AddPrimitive(f.AddMaterial("fs_opaque"));
        FramePipeline pipeline(
            f.device,
            ShadowShaped(PassDepthBias{}),
            BackBufferFormat,
            Width,
            Height
        );
        auto inputs = f.Inputs();

        f.Frame(pipeline, inputs);

        auto& map = *f.Pass("Shadow").depth->texture;
        EXPECT_EQ(
            halvesOf(f.Pass("Shadow").acquires, &map),
            TextureBarriers{MakeCrossSubmissionBarrier(
                map,
                SampledFragment,
                DepthWrite,
                true
            )}
        );
        EXPECT_EQ(
            halvesOf(f.Pass("Shadow").releases, &map),
            TextureBarriers{MakeBarrier(map, DepthWrite, SampledFragment)}
        );
        EXPECT_EQ(
            halvesOf(f.Pass("Opaque").acquires, &map),
            TextureBarriers{MakeBarrier(map, DepthWrite, SampledFragment)}
        );
        EXPECT_EQ(
            halvesOf(f.Pass("Opaque").releases, &map),
            TextureBarriers{MakeBarrier(map, SampledFragment, SampledFragment)}
        );
        EXPECT_EQ(
            halvesOf(f.Pass("Translucent").acquires, &map),
            TextureBarriers{MakeBarrier(map, SampledFragment, SampledFragment)}
        );
        EXPECT_EQ(
            f.device.textureCreates[1].usage,
            combine(
                RHITextureUsage::DepthStencil,
                RHITextureUsage::ShaderResource
            )
        );
        for(const auto* name:
            {"Shadow", "DepthPrepass", "Opaque", "Translucent"}) {
            const auto& pass = f.Pass(name);
            ASSERT_EQ(pass.pushes.size(), 1u) << name;
            EXPECT_EQ(pass.pushes[0].size(), sizeof(ScenePush)) << name;
        }
        EXPECT_TRUE(f.cmdList.violations.empty())
            << f.cmdList.violations.front().what;
        EXPECT_TRUE(f.cmdList.unconsumedAtClose.empty());
    }
}

TEST(FramePipeline, InvalidFullscreenDescsAreRefused) {
    constexpr FrameTargetID SceneDepth = 1;
    constexpr FrameTargetID SceneColor = 2;

    {
        auto desc = CompositeShaped();
        desc.passes[1].depth = DepthTargetUse{.target = SceneDepth};
        ExpectRefused(
            std::move(desc),
            "pass 'Composite': a fullscreen pass has no depth target"
        );
    }
    {
        auto desc = CompositeShaped();
        desc.passes.insert(
            desc.passes.begin() + 1,
            PassDesc{
                .name = "Blur",
                .reads = {SceneColor},
                .kind = FullscreenPassDesc{.fragmentShader = compositeShader}
            }
        );
        ExpectRefused(
            std::move(desc),
            "pass 'Blur': a fullscreen pass needs a color target"
        );
    }
    {
        auto desc = CompositeShaped();
        desc.passes[1].reads =
            {SceneColor, SceneDepth, SceneColor, SceneDepth, SceneColor};
        ExpectRefused(
            std::move(desc),
            "pass 'Composite': a fullscreen pass reads at most 4 targets"
        );
    }
    {
        auto desc = CompositeShaped();
        desc.passes[1].reads = {SceneColor, SceneColor};
        ExpectRefused(
            std::move(desc),
            "pass 'Composite': it reads target 2 twice"
        );
    }
    {
        auto desc = CompositeShaped();
        desc.passes[1].reads = {9};
        ExpectRefused(std::move(desc), "pass 'Composite': target 9 is unknown");
    }
    {
        auto desc = CompositeShaped();
        desc.passes[0].reads = {SceneColor};
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': it reads target 2, which it attaches"
        );
    }
    {
        auto desc = CompositeShaped();
        desc.passes[0].reads = {BackBufferTarget};
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': it reads the back buffer"
        );
    }
    {
        auto desc = CompositeShaped();
        desc.targets.push_back(
            FrameTargetDesc{
                .name = "Early",
                .format = RHIPixelFormat::RGBA8_UNORM
            }
        );
        desc.passes.insert(
            desc.passes.begin(),
            PassDesc{
                .name = "Early",
                .colors = {ColorTargetUse{
                    .target = static_cast<FrameTargetID>(desc.targets.size())
                }},
                .reads = {SceneColor},
                .kind = FullscreenPassDesc{.fragmentShader = compositeShader}
            }
        );
        ExpectRefused(
            std::move(desc),
            "pass 'Early': its first use of 'SceneColor' keeps contents "
            "nothing "
            "wrote"
        );
    }
}

// after the overlay pass, the list's last; and again after a Resize
TEST(FramePipeline, ACaptureAppendsOneBlitPassAtTheEndOfTheFrame) {
    using enum RHIResourceUsage;
    constexpr FrameTargetID SceneDepth = 1;
    constexpr FrameTargetID SceneColor = 3;
    constexpr u64 Frame = 7;

    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    auto desc = makeStandardPipeline({.shadowMapSize = 64});
    desc.targets.push_back(
        FrameTargetDesc{.name = "Unused", .format = RHIPixelFormat::RGBA8_UNORM}
    );
    FramePipeline
        pipeline(f.device, std::move(desc), BackBufferFormat, Width, Height);

    EXPECT_EQ(pipeline.FindTarget("SceneColor"), SceneColor);
    EXPECT_EQ(pipeline.FindTarget("SceneDepth"), SceneDepth);
    EXPECT_FALSE(pipeline.FindTarget("Unused").has_value());
    EXPECT_FALSE(pipeline.FindTarget("Missing").has_value());

    for(const auto [width, height]:
        {std::pair{Width, Height}, std::pair{128u, 64u}}) {
        pipeline.Resize(width, height);
        std::array captures{
            TargetReadback{
                .request = {.frame = Frame, .target = "SceneColor"},
                .target = SceneColor
            },
            TargetReadback{
                .request = {.frame = Frame, .target = "SceneDepth"},
                .target = SceneDepth
            }
        };
        auto inputs = f.Inputs();
        inputs.frame = Frame;
        inputs.captures = captures;

        f.Frame(pipeline, inputs);

        auto& color = *f.Pass("Opaque").colors[0].texture;
        auto& depth = *f.Pass("Opaque").depth->texture;
        ASSERT_EQ(f.cmdList.blitPasses.size(), 1u);
        const auto& blit = f.cmdList.blitPasses[0];
        EXPECT_EQ(blit.after, f.cmdList.passes.size());
        EXPECT_EQ(blit.event, "Capture");
        // Tonemap samples the colour last, Translucent attaches the depth last
        EXPECT_EQ(
            blit.acquires,
            (TextureBarriers{
                MakeBarrier(color, SampledFragment, CopySrc),
                MakeBarrier(depth, DepthWrite, CopySrc)
            })
        );
        EXPECT_EQ(
            blit.releases,
            (TextureBarriers{
                MakeBarrier(color, CopySrc, SampledFragment),
                MakeBarrier(depth, CopySrc, DepthWrite)
            })
        );
        EXPECT_TRUE(blit.bufferAcquires.empty());
        // left for Close, which puts each back into its last use's state
        ASSERT_EQ(f.cmdList.unconsumedAtClose.size(), 2u);
        for(const auto& release: blit.releases)
            EXPECT_TRUE(
                std::ranges::contains(f.cmdList.unconsumedAtClose, release)
            );

        ASSERT_EQ(blit.copies.size(), 2u);
        EXPECT_EQ(blit.copies[0].src, &color);
        EXPECT_EQ(blit.copies[1].src, &depth);
        EXPECT_EQ(captures[0].format, RHIPixelFormat::RGBA16_FLOAT);
        EXPECT_EQ(captures[1].format, RHIPixelFormat::D32_FLOAT);
        for(usize c = 0; c < captures.size(); ++c) {
            const auto& capture = captures[c];
            const auto& copy = blit.copies[c];
            const auto pitch = GetReadbackRowPitch(
                capture.format,
                width,
                FakeDevice::Capabilities
            );

            EXPECT_EQ(capture.recorded, Frame);
            EXPECT_EQ(capture.width, width);
            EXPECT_EQ(capture.height, height);
            EXPECT_EQ(capture.rowPitch, pitch);
            ASSERT_NE(capture.buffer, nullptr);
            EXPECT_EQ(capture.buffer->GetSize(), pitch * height);
            EXPECT_EQ(copy.dst, capture.buffer.get());
            EXPECT_EQ(copy.dstOffset, 0u);
            EXPECT_EQ(copy.dstRowPitch, pitch);
            EXPECT_EQ(copy.region.width, width);
            EXPECT_EQ(copy.region.height, height);
        }
        EXPECT_EQ(f.device.bufferCreates.back().memory, RHIMemoryType::CPURead);

        EXPECT_TRUE(f.cmdList.violations.empty())
            << f.cmdList.violations.front().what;
    }
}

TEST(FramePipeline, ACaptureStoresTheLastWriterThatFrameOnly) {
    using enum RHIResourceUsage;
    constexpr FrameTargetID SceneDepth = 1;
    constexpr usize Translucent = 2;

    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 0}),
        BackBufferFormat,
        Width,
        Height
    );
    const auto translucentStore = [&f] {
        return f.Pass("Translucent").depth->storeAction;
    };
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);
    EXPECT_EQ(translucentStore(), RHIStoreAction::DontCare);
    const auto releases = f.Pass("Translucent").releases;
    const auto edges = pipeline.Stats()[Translucent].barrierEdges;

    std::array captures{TargetReadback{
        .request = {.target = "SceneDepth"},
        .target = SceneDepth
    }};
    inputs.captures = captures;
    f.Frame(pipeline, inputs);

    EXPECT_EQ(translucentStore(), RHIStoreAction::Store);
    auto& depth = *f.Pass("Translucent").depth->texture;
    auto withCapture = releases;
    withCapture.push_back(MakeBarrier(depth, DepthWrite, CopySrc));
    EXPECT_EQ(f.Pass("Translucent").releases, withCapture);
    EXPECT_EQ(pipeline.Stats()[Translucent].barrierEdges, edges + 1);
    // the blit's edges belong to no pass
    EXPECT_EQ(pipeline.Stats().size(), 4u);

    inputs.captures = {};
    f.Frame(pipeline, inputs);

    EXPECT_EQ(translucentStore(), RHIStoreAction::DontCare);
    EXPECT_EQ(f.Pass("Translucent").releases, releases);
    EXPECT_EQ(pipeline.Stats()[Translucent].barrierEdges, edges);
    EXPECT_TRUE(f.cmdList.blitPasses.empty());
    EXPECT_TRUE(f.cmdList.violations.empty())
        << f.cmdList.violations.front().what;
}

TEST(FramePipeline, TheFrameAfterACaptureRecordsTheSameEdges) {
    using Layouts = std::vector<RHITextureLayout>;
    constexpr FrameTargetID SceneDepth = 1;
    constexpr FrameTargetID ShadowMap = 2;
    constexpr FrameTargetID SceneColor = 3;

    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({.shadowMapSize = 64}),
        BackBufferFormat,
        Width,
        Height
    );
    const auto layouts = [&f] {
        return Layouts{
            f.cmdList.LayoutOf(f.Pass("Opaque").depth->texture),
            f.cmdList.LayoutOf(f.Pass("Shadow").depth->texture),
            f.cmdList.LayoutOf(f.Pass("Opaque").colors[0].texture)
        };
    };
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);
    const auto before = f.cmdList.passes;
    const auto layoutsBefore = layouts();

    std::array captures{
        TargetReadback{
            .request = {.target = "SceneDepth"},
            .target = SceneDepth
        },
        TargetReadback{.request = {.target = "ShadowMap"}, .target = ShadowMap},
        TargetReadback{
            .request = {.target = "SceneColor"},
            .target = SceneColor
        }
    };
    inputs.captures = captures;
    f.Frame(pipeline, inputs);
    // Close completed each release: every target is where its last use left
    // it, which the next frame's first acquire names
    EXPECT_EQ(layouts(), layoutsBefore);
    EXPECT_EQ(f.cmdList.unconsumedAtClose.size(), captures.size());

    inputs.captures = {};
    f.Frame(pipeline, inputs);

    const auto& after = f.cmdList.passes;
    ASSERT_EQ(after.size(), before.size());
    for(usize i = 0; i < after.size(); ++i) {
        EXPECT_EQ(after[i].acquires, before[i].acquires) << after[i].event;
        EXPECT_EQ(after[i].releases, before[i].releases) << after[i].event;
    }
    EXPECT_EQ(layouts(), layoutsBefore);
    EXPECT_TRUE(f.cmdList.blitPasses.empty());
    EXPECT_TRUE(f.cmdList.unconsumedAtClose.empty());
    EXPECT_TRUE(f.cmdList.violations.empty())
        << f.cmdList.violations.front().what;
}
