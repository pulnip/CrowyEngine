#pragma once

#include <optional>
#include <string>
#include <vector>
#include "RenderDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct GraphicsPipelineBind{
        std::string name;

        ShaderBindSpec vs;
        ShaderBindSpec fs;

        RHIGraphicsPipelineStateRAII pso;
        std::optional<RenderTypeHash> renderType;

        inline bool isFullscreenPass() const{
            return !renderType.has_value();
        }
    };

    struct RenderPass{
        std::string name;
        bool enabled = true;

        // output Texture
        std::vector<std::string> outputs;
        std::string depthOutput;

        std::vector<GraphicsPipelineBind> pipelines;
    };
}