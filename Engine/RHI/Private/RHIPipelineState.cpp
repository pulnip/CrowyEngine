#include "RHIPipelineState.hpp"

#include <format>
#include <stdexcept>

#include "PathUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    void validateGraphicsPipelineDesc(
        const RHIGraphicsPipelineStateDesc& desc
    ) {
        // a fragment stage without render targets stays legal: a masked depth
        // pass
        if(desc.fragmentShader || desc.renderTargetCount == 0) {
            return;
        }

        throw std::runtime_error(
            std::format(
                "a pipeline with render targets needs a fragment shader ({})",
                desc.vertexShader.path
            )
        );
    }
}
