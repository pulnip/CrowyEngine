#include <optional>
#include <stdexcept>

#include <gtest/gtest.h>

#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"

using namespace Crowy;

namespace
{
    // one render target and a fragment stage, as every color pass has
    RHIGraphicsPipelineStateDesc colorDesc() {
        return RHIGraphicsPipelineStateDesc{
            .vertexShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "vs_main"},
            .fragmentShader =
                RHIShaderDesc{
                    .path = "Engine/Shader/X.slang",
                    .entryPoint = "fs_main"
                },
            .renderTargetFormats = {RHIPixelFormat::RGBA8_UNORM},
            .renderTargetCount = 1
        };
    }
}

TEST(PipelineRules, RenderTargetsWithoutAFragmentStageThrow) {
    auto desc = colorDesc();
    desc.fragmentShader = std::nullopt;

    try {
        validateGraphicsPipelineDesc(desc);
        FAIL() << "validateGraphicsPipelineDesc did not throw";
    }
    catch(const std::runtime_error& e) {
        EXPECT_STREQ(
            e.what(),
            "a pipeline with render targets needs a fragment shader "
            "(Engine/Shader/X.slang)"
        );
    }
}

TEST(PipelineRules, ADepthOnlyDescIsValid) {
    auto desc = colorDesc();
    desc.fragmentShader = std::nullopt;
    desc.renderTargetCount = 0;

    EXPECT_NO_THROW(validateGraphicsPipelineDesc(desc));
}

// a masked depth pass will want one
TEST(PipelineRules, AFragmentStageWithoutRenderTargetsIsValid) {
    auto desc = colorDesc();
    desc.renderTargetCount = 0;

    EXPECT_NO_THROW(validateGraphicsPipelineDesc(desc));
}
