#include <array>
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
        translucent.depthWrite = false;
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
            "Engine/Shader/X.slang (vs_main, fs_main): fake compile error"
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
    wireframe.fillMode = RHIFillMode::Wireframe;
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
