#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "PipelineCache.hpp"

using namespace Crowy;

namespace
{
    constexpr std::array Formats = {RHIPixelFormat::RGBA8_UNORM};

    // one file and one pair of entry points, so every key names the same
    // shaders and a failure's message does not depend on the map's order
    MaterialPipelineDesc OpaqueMaterial() {
        return MaterialPipelineDesc{
            .vertexShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "vs_main"},
            .fragmentShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "fs_main"},
            .profile = "sm_6_8"
        };
    }

    std::array<MaterialPipelineDesc, 3> ThreeMaterials() {
        auto doubleSided = OpaqueMaterial();
        doubleSided.rasterizer.cullMode = RHICullMode::None;

        auto translucent = OpaqueMaterial();
        translucent.domain = MaterialDomain::Translucent;
        translucent.blend = RHIBlendState{};

        return {OpaqueMaterial(), doubleSided, translucent};
    }

    PassPipelineDesc BasePass() {
        return PassPipelineDesc{
            .renderTargetFormats = Formats,
            .depthFormat = RHIPixelFormat::D32_FLOAT
        };
    }

    std::vector<RHIGraphicsPipelineState*> ResolveAll(PipelineCache& cache) {
        std::vector<RHIGraphicsPipelineState*> states;
        for(const auto& material: ThreeMaterials())
            states.push_back(&cache.Resolve(material, BasePass()));

        return states;
    }
}

// The swap happens only after every key built, and the old states go to the
// device's retire queue rather than dying while a frame may still bind them.
TEST(PipelineCache, RebuildRecreatesEveryKeyAndRetiresTheOld) {
    FakeDevice device;
    PipelineCache cache(device);
    const auto before = ResolveAll(cache);

    const auto rebuild = cache.Rebuild();
    const auto after = ResolveAll(cache);

    EXPECT_EQ(rebuild.pipelines, 3u);
    EXPECT_EQ(cache.Count(), 3u);
    EXPECT_EQ(device.creates, 6u);
    for(usize i = 0; i < after.size(); ++i)
        EXPECT_NE(after[i], before[i]);
    EXPECT_EQ(device.destroyed, 0u);
    EXPECT_EQ(device.deferred.size(), 3u);

    device.RunDeferred();
    EXPECT_EQ(device.destroyed, 3u);
}

TEST(PipelineCache, RebuildKeepsTheOldStatesWhenOneFails) {
    FakeDevice device;
    PipelineCache cache(device);
    const auto before = ResolveAll(cache);
    // the rebuild's second create
    device.failAt = device.creates + 2;

    try {
        cache.Rebuild();
        FAIL() << "Rebuild did not throw";
    } catch(const std::runtime_error& e) {
        EXPECT_STREQ(
            e.what(),
            "Engine/Shader/X.slang (vs_main, fs_main) + Engine/Shader/PBR.slang: "
            "fake compile error"
        );
    }
    const auto after = ResolveAll(cache);

    EXPECT_EQ(cache.Count(), 3u);
    EXPECT_EQ(after, before);
    EXPECT_TRUE(device.deferred.empty());
    // the one state built before the failure was never bound, so it is
    // dropped at once rather than retired
    EXPECT_EQ(device.destroyed, 1u);
}

TEST(PipelineCache, RebuildOfAnEmptyCacheCreatesNothing) {
    FakeDevice device;
    PipelineCache cache(device);

    const auto rebuild = cache.Rebuild();

    EXPECT_EQ(rebuild.pipelines, 0u);
    EXPECT_EQ(device.creates, 0u);
    EXPECT_TRUE(device.deferred.empty());
}

// A debug view's override keys every material apart from its plain state, so
// the cache holds both variants side by side and toggling back costs nothing.
TEST(PipelineCache, AnOverriddenPassResolvesItsOwnVariants) {
    FakeDevice device;
    PipelineCache cache(device);
    const auto plain = ResolveAll(cache);

    auto wireframe = BasePass();
    wireframe.debug.fillMode = RHIFillMode::Wireframe;
    std::vector<RHIGraphicsPipelineState*> variants;
    for(const auto& material: ThreeMaterials())
        variants.push_back(&cache.Resolve(material, wireframe));

    EXPECT_EQ(cache.Count(), 6u);
    EXPECT_EQ(device.creates, 6u);
    for(usize i = 0; i < variants.size(); ++i)
        EXPECT_NE(variants[i], plain[i]);
    EXPECT_EQ(ResolveAll(cache), plain);
    EXPECT_EQ(device.creates, 6u);
}

// Playground's prepass: grid, opaque and double-sided materials rasterize
// in two ways, so the depth-only pass compiles two pipelines, not three.
TEST(PipelineCache, ADepthOnlyPassSharesKeysAcrossFragmentShaders) {
    FakeDevice device;
    PipelineCache cache(device);

    auto grid = OpaqueMaterial();
    grid.fragmentShader.entryPoint = "fs_grid";
    auto doubleSided = OpaqueMaterial();
    doubleSided.rasterizer.cullMode = RHICullMode::None;

    const PassPipelineDesc prepass{.depthFormat = RHIPixelFormat::D32_FLOAT};
    auto& gridState = cache.Resolve(grid, prepass);
    auto& opaqueState = cache.Resolve(OpaqueMaterial(), prepass);
    cache.Resolve(doubleSided, prepass);

    EXPECT_EQ(cache.Count(), 2u);
    EXPECT_EQ(device.creates, 2u);
    EXPECT_EQ(&gridState, &opaqueState);
}

// A fullscreen pass's desc names no material: the cache keys it by value,
// shares a key with an equal composed desc, and a reload rebuilds it too.
TEST(PipelineCache, ARawDescIsKeyedAndRebuiltLikeAComposedOne) {
    FakeDevice device;
    PipelineCache cache(device);
    auto& composed = cache.Resolve(OpaqueMaterial(), BasePass());

    EXPECT_EQ(&cache.Resolve(Compose(OpaqueMaterial(), BasePass())), &composed);

    auto raw = Compose(OpaqueMaterial(), BasePass());
    raw.depthStencil = std::nullopt;
    raw.rasterizer.cullMode = RHICullMode::None;
    auto& fullscreen = cache.Resolve(raw);
    EXPECT_EQ(&cache.Resolve(raw), &fullscreen);
    EXPECT_EQ(cache.Count(), 2u);
    EXPECT_EQ(device.creates, 2u);

    const auto rebuild = cache.Rebuild();

    EXPECT_EQ(rebuild.pipelines, 2u);
    EXPECT_EQ(device.creates, 4u);
    EXPECT_EQ(device.deferred.size(), 2u);
    EXPECT_NE(&cache.Resolve(raw), &fullscreen);
    EXPECT_EQ(device.creates, 4u);
}

// the module is where a model's error lies, so a failed rebuild names it
TEST(PipelineCache, RebuildNamesTheLinkedModule) {
    FakeDevice device;
    PipelineCache cache(device);
    auto toon = OpaqueMaterial();
    toon.shadingModule = "Engine/Shader/Toon.slang";
    cache.Resolve(toon, BasePass());
    // the rebuild's first create
    device.failAt = device.creates + 1;

    try {
        cache.Rebuild();
        FAIL() << "Rebuild did not throw";
    } catch(const std::runtime_error& e) {
        EXPECT_STREQ(
            e.what(),
            "Engine/Shader/X.slang (vs_main, fs_main) + "
            "Engine/Shader/Toon.slang: fake compile error"
        );
    }
}

// Every model shares the prepass's and the shadow's pipeline, and each keys
// its own in a colour pass.
TEST(PipelineCache, ModelsShareTheDepthOnlyPipeline) {
    FakeDevice device;
    PipelineCache cache(device);
    auto toon = OpaqueMaterial();
    toon.shadingModule = "Engine/Shader/Toon.slang";

    const PassPipelineDesc prepass{.depthFormat = RHIPixelFormat::D32_FLOAT};
    auto& pbrDepth = cache.Resolve(OpaqueMaterial(), prepass);
    auto& toonDepth = cache.Resolve(toon, prepass);

    EXPECT_EQ(&pbrDepth, &toonDepth);
    EXPECT_EQ(device.creates, 1u);
    EXPECT_TRUE(device.pipelineCreates.back().linkedModules.empty());

    auto& pbrColour = cache.Resolve(OpaqueMaterial(), BasePass());
    auto& toonColour = cache.Resolve(toon, BasePass());

    EXPECT_NE(&pbrColour, &toonColour);
    EXPECT_EQ(device.creates, 3u);
    EXPECT_EQ(
        device.pipelineCreates.back().linkedModules,
        std::vector<std::filesystem::path>{"Engine/Shader/Toon.slang"}
    );
}
