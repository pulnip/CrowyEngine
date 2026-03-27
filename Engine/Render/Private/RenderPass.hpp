#pragma once

#include <optional>
#include <string>
#include <vector>
#include "RenderDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct PipelineBind{
        std::string name;
        // input Texture
        std::vector<std::string> inputs;
        // output Texture
        std::vector<std::string> outputs;
        std::string depthOutput;

        std::vector<BindSpec> fs_samplers;
        std::vector<BindSpec> fs_cbuffers;

        RHIPipelineStatePtr pso;
        std::optional<RenderTypeHash> renderType;

        inline bool isFullscreenPass() const{
            return !renderType.has_value();
        }
    };

    struct RenderPass{
        std::string name;
        bool enabled = true;
        std::vector<PipelineBind> pipelines;
    };
}