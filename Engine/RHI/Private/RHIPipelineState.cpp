#include "RHIPipelineState.hpp"

#include <format>
#include <stdexcept>
#include <variant>

#include "RHIDefinitions.hpp"

namespace Crowy
{
    void ValidateGraphicsPipelineDesc(
        const RHIGraphicsPipelineStateDesc& desc
    ) {
        // a fragment stage without render targets stays legal: a masked depth
        // pass
        if(desc.fragmentShader || desc.renderTargetCount == 0) {
            return;
        }

        // both backends refuse a mesh frontend before they construct a pipeline
        const auto& frontend =
            std::get<RHILegacyFrontendDesc>(desc.preRasterizer);
        throw std::runtime_error(
            std::format(
                "a pipeline with render targets needs a fragment shader ({})",
                frontend.vertexShader.path
            )
        );
    }
}
