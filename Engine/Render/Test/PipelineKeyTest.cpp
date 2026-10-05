#include <array>
#include <filesystem>
#include <optional>
#include <vector>

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

// The cache compares only inside a bucket, so equal keys must also hash alike.
// A null profile is a key too: RHIShader reads it as "no profile".
TEST(PipelineKey, ProfileHashesByValueNotAddress) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};
    const auto hash = std::hash<RHIGraphicsPipelineStateDesc>{};

    auto material = OpaqueMaterial();
    static char otherProfile[] = "sm_6_8";
    material.profile = otherProfile;

    auto lhs = Compose(OpaqueMaterial(), BasePass(formats));
    auto rhs = Compose(material, BasePass(formats));

    ASSERT_NE(lhs.profile, rhs.profile);
    EXPECT_EQ(hash(lhs), hash(rhs));

    lhs.profile = nullptr;
    rhs.profile = nullptr;
    EXPECT_EQ(hash(lhs), hash(rhs));
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

// The layout hash mixes in each element's hash, so the element has to hash
// what semanticName says, like its comparison does.
TEST(PipelineKey, VertexElementHashesByValueNotAddress) {
    const auto hash = std::hash<RHIVertexElement>{};

    static char position[] = "POSITION";
    RHIVertexElement lhs{
        .semanticName = position,
        .semanticIndex = 0,
        .format = RHIPixelFormat::RGB32_FLOAT,
        .inputSlot = 0,
        .alignedByteOffset = 0,
        .classification = RHIInputClassification::PerVertex,
        .instanceDataStepRate = 0
    };
    auto rhs = lhs;
    rhs.semanticName = "POSITION";

    ASSERT_NE(lhs.semanticName, rhs.semanticName);
    EXPECT_EQ(hash(lhs), hash(rhs));

    lhs.semanticName = nullptr;
    rhs.semanticName = nullptr;
    EXPECT_EQ(hash(lhs), hash(rhs));
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
    translucent.domain = MaterialDomain::Translucent;
    translucent.blend = RHIBlendState{};

    const auto opaque = Compose(OpaqueMaterial(), BasePass(formats));
    EXPECT_NE(opaque, Compose(doubleSided, BasePass(formats)));
    EXPECT_NE(opaque, Compose(translucent, BasePass(formats)));
}

// The overdraw view's shape: every material, the translucent one included,
// adds without testing or writing depth, whatever it or the pass asked for.
TEST(PipelineKey, PassOverridesReplaceTheMaterialState) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto translucent = OpaqueMaterial();
    translucent.domain = MaterialDomain::Translucent;
    RHIBlendState alpha{};
    alpha.renderTargets[0] = RHIRenderTargetBlendState{
        .blendEnable = true,
        .srcBlend = RHIBlend::SrcAlpha,
        .dstBlend = RHIBlend::InvSrcAlpha
    };
    translucent.blend = alpha;

    RHIBlendState additive{};
    additive.renderTargets[0] = RHIRenderTargetBlendState{
        .blendEnable = true,
        .srcBlend = RHIBlend::One,
        .dstBlend = RHIBlend::One
    };
    // the opaque pass after a prepass, which the override still beats
    auto overdraw = BasePass(formats);
    overdraw.state.depthFunc = RHIComparisonFunc::Equal;
    overdraw.state.depthWrite = false;
    overdraw.debug = MeshPassOverride{
        .fillMode = RHIFillMode::Wireframe,
        .blend = additive,
        .depthFunc = RHIComparisonFunc::Always,
        .depthWrite = false
    };

    for(const auto& material: {OpaqueMaterial(), translucent}) {
        const auto desc = Compose(material, overdraw);

        EXPECT_EQ(desc.rasterizer.fillMode, RHIFillMode::Wireframe);
        ASSERT_TRUE(desc.blend.has_value());
        EXPECT_EQ(*desc.blend, additive);
        // Metal blends whatever this says, D3D12 honours it
        EXPECT_TRUE(desc.blend->renderTargets[0].blendEnable);
        ASSERT_TRUE(desc.depthStencil.has_value());
        EXPECT_EQ(desc.depthStencil->format, RHIPixelFormat::D32_FLOAT);
        EXPECT_EQ(desc.depthStencil->depthFunc, RHIComparisonFunc::Always);
        EXPECT_FALSE(desc.depthStencil->depthWriteEnable);
        EXPECT_NE(desc, Compose(material, BasePass(formats)));
    }
}

// the material's rasterizer and blend, the pass's depth
TEST(PipelineKey, APassWithoutOverridesKeepsTheMaterialState) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto material = OpaqueMaterial();
    material.rasterizer.fillMode = RHIFillMode::Wireframe;
    material.blend = RHIBlendState{};

    auto pass = BasePass(formats);
    pass.state.depthFunc = RHIComparisonFunc::LessEqual;
    pass.state.depthWrite = false;

    const auto desc = Compose(material, pass);

    EXPECT_EQ(desc.rasterizer, material.rasterizer);
    EXPECT_EQ(desc.blend, material.blend);
    ASSERT_TRUE(desc.depthStencil.has_value());
    EXPECT_EQ(desc.depthStencil->depthFunc, RHIComparisonFunc::LessEqual);
    EXPECT_FALSE(desc.depthStencil->depthWriteEnable);
}

// A masked depth pass keeps its fragment stage and an opaque one drops it;
// with no render targets on either side, only the stage keys them apart.
TEST(PipelineKey, AnAbsentFragmentStageKeysApart) {
    const auto withoutStage = Compose(OpaqueMaterial(), BasePass({}));
    auto withStage = withoutStage;
    withStage.fragmentShader = OpaqueMaterial().fragmentShader;

    ASSERT_EQ(withoutStage.renderTargetCount, 0u);
    ASSERT_FALSE(withoutStage.fragmentShader.has_value());
    EXPECT_NE(withStage, withoutStage);
}

TEST(PipelineKey, AbsentFragmentStagesCompareAndHashAlike) {
    const auto hash = std::hash<RHIGraphicsPipelineStateDesc>{};

    auto lhs = Compose(OpaqueMaterial(), BasePass({}));
    auto rhs = Compose(OpaqueMaterial(), BasePass({}));
    lhs.fragmentShader = std::nullopt;
    rhs.fragmentShader = std::nullopt;

    EXPECT_EQ(lhs, rhs);
    EXPECT_EQ(hash(lhs), hash(rhs));
}

// a pass with render targets and no fragment override draws the material's
TEST(PipelineKey, ComposeKeepsTheMaterialFragmentShader) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    const auto desc = Compose(OpaqueMaterial(), BasePass(formats));

    ASSERT_TRUE(desc.fragmentShader.has_value());
    EXPECT_EQ(*desc.fragmentShader, OpaqueMaterial().fragmentShader);
}

// The split the prepass needs: one material, a depth-only pass writing depth
// and a color pass testing Equal against it, two pipelines.
TEST(PipelineKey, OneMaterialKeysApartInTwoPasses) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto opaque = BasePass(formats);
    opaque.state.depthFunc = RHIComparisonFunc::Equal;
    opaque.state.depthWrite = false;

    const auto prepassDesc = Compose(OpaqueMaterial(), BasePass({}));
    const auto opaqueDesc = Compose(OpaqueMaterial(), opaque);

    EXPECT_NE(prepassDesc, opaqueDesc);
    EXPECT_EQ(prepassDesc.preRasterizer, opaqueDesc.preRasterizer);
    ASSERT_TRUE(prepassDesc.depthStencil.has_value());
    EXPECT_EQ(prepassDesc.depthStencil->depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(prepassDesc.depthStencil->depthWriteEnable);
    ASSERT_TRUE(opaqueDesc.depthStencil.has_value());
    EXPECT_EQ(opaqueDesc.depthStencil->depthFunc, RHIComparisonFunc::Equal);
    EXPECT_FALSE(opaqueDesc.depthStencil->depthWriteEnable);
    EXPECT_TRUE(opaqueDesc.fragmentShader.has_value());
}

// Playground's grid and opaque materials differ only in fragment entry, and
// an opaque material may set a blend: a depth-only pass keys neither, and
// ignores a debug override.
TEST(PipelineKey, APassWithoutTargetsHasNoFragmentStage) {
    const auto hash = std::hash<RHIGraphicsPipelineStateDesc>{};

    auto grid = OpaqueMaterial();
    grid.fragmentShader.entryPoint = "fs_grid";
    grid.blend = RHIBlendState{};

    auto prepass = BasePass({});
    const auto opaqueDesc = Compose(OpaqueMaterial(), prepass);
    const auto gridDesc = Compose(grid, prepass);
    prepass.debug.fillMode = RHIFillMode::Wireframe;
    prepass.debug.depthFunc = RHIComparisonFunc::Always;

    EXPECT_FALSE(opaqueDesc.fragmentShader.has_value());
    EXPECT_FALSE(gridDesc.blend.has_value());
    EXPECT_EQ(opaqueDesc.renderTargetCount, 0u);
    EXPECT_EQ(opaqueDesc, gridDesc);
    EXPECT_EQ(hash(opaqueDesc), hash(gridDesc));
    EXPECT_EQ(Compose(OpaqueMaterial(), prepass), opaqueDesc);
}

TEST(PipelineKey, APassFragmentShaderReplacesTheMaterials) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};
    const RHIShaderDesc normals{
        .path = "Engine/Shader/Y.slang",
        .entryPoint = "fs_normals"
    };

    auto pass = BasePass(formats);
    pass.state.fragmentShader = normals;
    const auto desc = Compose(OpaqueMaterial(), pass);

    ASSERT_TRUE(desc.fragmentShader.has_value());
    EXPECT_EQ(*desc.fragmentShader, normals);
    EXPECT_NE(desc, Compose(OpaqueMaterial(), BasePass(formats)));
}

// the ladder's second rung and the shadow pass's bias, in either kind of pass
TEST(PipelineKey, PassDepthBiasReplacesTheMaterialsBias) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto material = OpaqueMaterial();
    material.rasterizer.depthBias = 3;
    material.rasterizer.depthBiasClamp = 0.5f;
    material.rasterizer.slopeScaledDepthBias = 1.5f;
    material.rasterizer.cullMode = RHICullMode::None;

    for(auto pass: {BasePass({}), BasePass(formats)}) {
        pass.state.depthBias = PassDepthBias{.depthBias = -1};
        const auto desc = Compose(material, pass);

        EXPECT_EQ(desc.rasterizer.depthBias, -1);
        EXPECT_EQ(desc.rasterizer.depthBiasClamp, 0.0f);
        EXPECT_EQ(desc.rasterizer.slopeScaledDepthBias, 0.0f);
        EXPECT_EQ(desc.rasterizer.cullMode, RHICullMode::None);
    }
}

// The model is a path compared by value, like the shader paths: a material
// naming the default links what one naming nothing new links, and another
// model keys apart.
TEST(PipelineKey, ShadingModuleKeysByValue) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};
    const auto hash = std::hash<RHIGraphicsPipelineStateDesc>{};

    auto pbr = OpaqueMaterial();
    pbr.shadingModule = Str{"Engine/Shader/PBR.slang"};
    auto toon = OpaqueMaterial();
    toon.shadingModule = "Engine/Shader/Toon.slang";

    const auto defaultDesc = Compose(OpaqueMaterial(), BasePass(formats));
    const auto pbrDesc = Compose(pbr, BasePass(formats));
    const auto toonDesc = Compose(toon, BasePass(formats));

    EXPECT_EQ(
        defaultDesc.linkedModules,
        std::vector<std::filesystem::path>{PBRShadingModule}
    );
    EXPECT_EQ(pbrDesc, defaultDesc);
    EXPECT_EQ(hash(pbrDesc), hash(defaultDesc));
    EXPECT_NE(toonDesc, defaultDesc);
    EXPECT_EQ(
        toonDesc.linkedModules,
        std::vector<std::filesystem::path>{"Engine/Shader/Toon.slang"}
    );
}

// A depth-only program has no fragment stage to call a model from, so a Toon
// and a PBR material that rasterize alike share its one key.
TEST(PipelineKey, ADepthOnlyPassDropsTheModule) {
    const auto hash = std::hash<RHIGraphicsPipelineStateDesc>{};

    auto toon = OpaqueMaterial();
    toon.shadingModule = "Engine/Shader/Toon.slang";

    const auto pbrDesc = Compose(OpaqueMaterial(), BasePass({}));
    const auto toonDesc = Compose(toon, BasePass({}));

    EXPECT_TRUE(pbrDesc.linkedModules.empty());
    EXPECT_EQ(pbrDesc, toonDesc);
    EXPECT_EQ(hash(pbrDesc), hash(toonDesc));
}

// an empty path links nothing, and neither does a colour pass whose fragment
// override never calls the model, as a normals prepass's
TEST(PipelineKey, AMaterialWithoutAModuleLinksNothing) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};

    auto unlinked = OpaqueMaterial();
    unlinked.shadingModule.clear();
    EXPECT_TRUE(Compose(unlinked, BasePass(formats)).linkedModules.empty());

    auto normals = BasePass(formats);
    normals.state.fragmentShader = RHIShaderDesc{
        .path = "Engine/Shader/Y.slang",
        .entryPoint = "fs_normals"
    };
    normals.state.linksShading = false;
    const auto desc = Compose(OpaqueMaterial(), normals);

    EXPECT_TRUE(desc.linkedModules.empty());
    ASSERT_TRUE(desc.fragmentShader.has_value());
    EXPECT_EQ(desc.fragmentShader->entryPoint, "fs_normals");
}

// the depth-only passes run a Masked material's cut and nothing of its color
TEST(PipelineKey, AMaskedMaterialRunsItsMaskEntryInADepthOnlyPass) {
    auto masked = OpaqueMaterial();
    masked.domain = MaterialDomain::Masked;
    masked.maskShader = {
        .path = "Engine/Shader/X.slang",
        .entryPoint = "fs_masked_depth"
    };
    masked.shadingModule = "Engine/Shader/Toon.slang";
    masked.blend = RHIBlendState{};

    auto prepass = BasePass({});
    const auto desc = Compose(masked, prepass);
    ASSERT_TRUE(desc.fragmentShader.has_value());
    EXPECT_EQ(*desc.fragmentShader, masked.maskShader);
    EXPECT_EQ(desc.renderTargetCount, 0u);
    EXPECT_TRUE(desc.linkedModules.empty());
    EXPECT_FALSE(desc.blend.has_value());
    EXPECT_TRUE(desc.depthStencil->depthWriteEnable);
    EXPECT_EQ(desc.depthStencil->depthFunc, RHIComparisonFunc::Less);
    EXPECT_NE(desc, Compose(OpaqueMaterial(), prepass));

    // debug overrides never reach a depth-only pass
    prepass.debug.fillMode = RHIFillMode::Wireframe;
    EXPECT_EQ(Compose(masked, prepass), desc);
}

// an Opaque material's mask entry reaches no key, so no existing key moves
TEST(PipelineKey, AnOpaqueMaterialNeverRunsItsMaskEntry) {
    const auto hash = std::hash<RHIGraphicsPipelineStateDesc>{};
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};
    auto other = OpaqueMaterial();
    other.maskShader = {
        .path = "Engine/Shader/Y.slang",
        .entryPoint = "fs_other"
    };

    for(const auto& pass: {BasePass({}), BasePass(formats)}) {
        EXPECT_EQ(Compose(other, pass), Compose(OpaqueMaterial(), pass));
        EXPECT_EQ(
            hash(Compose(other, pass)),
            hash(Compose(OpaqueMaterial(), pass))
        );
    }
}

// in a color pass a Masked material draws its own entry, shaded like any;
// a pass override replaces it, cut and all
TEST(PipelineKey, AMaskedMaterialDrawsItsOwnEntryInAColorPass) {
    const std::array formats = {RHIPixelFormat::RGBA8_UNORM};
    auto masked = OpaqueMaterial();
    masked.domain = MaterialDomain::Masked;
    masked.fragmentShader.entryPoint = "fs_masked";
    masked.maskShader = {
        .path = "Engine/Shader/X.slang",
        .entryPoint = "fs_masked_depth"
    };

    auto pass = BasePass(formats);
    pass.state.depthFunc = RHIComparisonFunc::Equal;
    pass.state.depthWrite = false;
    const auto desc = Compose(masked, pass);
    ASSERT_TRUE(desc.fragmentShader.has_value());
    EXPECT_EQ(desc.fragmentShader->entryPoint, "fs_masked");
    EXPECT_EQ(
        desc.linkedModules,
        std::vector<std::filesystem::path>{PBRShadingModule}
    );

    pass.state.fragmentShader = RHIShaderDesc{
        .path = "Engine/Shader/Y.slang",
        .entryPoint = "fs_normals"
    };
    pass.state.linksShading = false;
    EXPECT_EQ(Compose(masked, pass).fragmentShader->entryPoint, "fs_normals");
}
