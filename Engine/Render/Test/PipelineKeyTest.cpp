#include <array>

#include <gtest/gtest.h>

#include "PipelineCache.hpp"

using namespace Crowy;

namespace
{
    // Deliberately built from separate literals: the point of every test here
    // is that the key compares what strings say, not where they live.
    MaterialPipelineDesc OpaqueMaterial() {
        return MaterialPipelineDesc{
            .vertexShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "vs_main"},
            .fragmentShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "fs_main"},
            .profile = "sm_6_8"
        };
    }

    PassPipelineDesc BasePass(std::span<const RHIPixelFormat> formats) {
        return PassPipelineDesc{
            .renderTargetFormats = formats,
            .depthFormat = RHIPixelFormat::D32_FLOAT
        };
    }
}

TEST(PipelineKey, SameMaterialAndPassCompareEqual) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    const auto lhs = Compose(OpaqueMaterial(), BasePass(formats));
    const auto rhs = Compose(OpaqueMaterial(), BasePass(formats));

    EXPECT_EQ(lhs, rhs);
    EXPECT_EQ(
        std::hash<RHIGraphicsPipelineStateDesc>{}(lhs),
        std::hash<RHIGraphicsPipelineStateDesc>{}(rhs)
    );
}

// The trap the handoff names: `profile` is a CStr, so a defaulted comparison
// would compile the same pipeline twice for two "sm_6_8" literals that landed
// at different addresses.
TEST(PipelineKey, ProfileComparesByValueNotAddress) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto material = OpaqueMaterial();
    // a separate array, so the compiler cannot pool it with the literal above
    static char otherProfile[] = "sm_6_8";
    material.profile = otherProfile;

    const auto lhs = Compose(OpaqueMaterial(), BasePass(formats));
    const auto rhs = Compose(material, BasePass(formats));

    ASSERT_NE(OpaqueMaterial().profile, material.profile);
    EXPECT_EQ(lhs, rhs);
}

// The one the handoff missed: RHIVertexElement::semanticName is a CStr too.
// Vertex pulling took the layout out of MaterialPipelineDesc, but the RHI type
// is still a pipeline key, so the comparison still has to hold.
TEST(PipelineKey, VertexLayoutComparesItsElements) {
    static char position[] = "POSITION";
    const std::array layout = {RHIVertexElement{
        .semanticName = position,
        .semanticIndex = 0,
        .format = RHIPixelFormat::RGB32_FLOAT,
        .inputSlot = 0,
        .alignedByteOffset = 0,
        .classification = RHIInputClassification::PerVertex,
        .instanceDataStepRate = 0
    }};
    const std::array sameLayout = {RHIVertexElement{
        .semanticName = "POSITION",
        .semanticIndex = 0,
        .format = RHIPixelFormat::RGB32_FLOAT,
        .inputSlot = 0,
        .alignedByteOffset = 0,
        .classification = RHIInputClassification::PerVertex,
        .instanceDataStepRate = 0
    }};

    const RHILegacyFrontendDesc lhs{.vertexLayout = layout};
    const RHILegacyFrontendDesc rhs{.vertexLayout = sameLayout};
    const RHILegacyFrontendDesc none{};

    ASSERT_NE(layout[0].semanticName, sameLayout[0].semanticName);
    EXPECT_EQ(lhs, rhs);
    EXPECT_NE(lhs, none);
}

// Vertex pulling's payoff: a mesh's attribute set never reaches the key, so
// two materials that once needed two pipelines now share one.
TEST(PipelineKey, MeshAttributesDoNotReachTheKey) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};
    const auto desc = Compose(OpaqueMaterial(), BasePass(formats));

    const auto& frontend = std::get<RHILegacyFrontendDesc>(desc.preRasterizer);
    EXPECT_FALSE(frontend.vertexLayout.has_value());
}

// The reason MaterialPipelineDesc carries no render target state: the same
// material in two passes has to key to two pipelines.
TEST(PipelineKey, PassStateSeparatesTheSameMaterial) {
    const std::array base = {RHIPixelFormat::RGBA8_UNORM};
    const std::array hdr = {RHIPixelFormat::RGBA16_FLOAT};

    EXPECT_NE(
        Compose(OpaqueMaterial(), BasePass(base)),
        Compose(OpaqueMaterial(), BasePass(hdr))
    );

    auto prepass = BasePass(base);
    prepass.depthFormat = RHIPixelFormat::D24_UNORM_S8_UINT;
    EXPECT_NE(
        Compose(OpaqueMaterial(), BasePass(base)),
        Compose(OpaqueMaterial(), prepass)
    );
}

TEST(PipelineKey, MaterialStateSeparatesPipelines) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto doubleSided = OpaqueMaterial();
    doubleSided.rasterizer.cullMode = RHICullMode::None;

    auto translucent = OpaqueMaterial();
    translucent.depthWrite = false;
    translucent.blend = RHIBlendState{};

    const auto opaque = Compose(OpaqueMaterial(), BasePass(formats));
    EXPECT_NE(opaque, Compose(doubleSided, BasePass(formats)));
    EXPECT_NE(opaque, Compose(translucent, BasePass(formats)));
}
