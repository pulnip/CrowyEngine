#pragma once

#include <string>
#include <vector>
#include "RHIDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct ComputePipelineBind{
        std::string name;
        // input
        std::vector<BindSpec> inputTextures;
        std::vector<BindSpec> inputBuffers;

        // output
        std::vector<BindSpec> outputTextures;
        std::vector<BindSpec> outputBuffers;

        RHIPipelineStatePtr pso;
        RHISize3D gridSize;

        inline size_t numOutputs() const{
            return outputTextures.size() + outputBuffers.size();
        }
    };

    struct ComputePass{
        std::string name;
        bool enabled = true;
        std::vector<ComputePipelineBind> pipelines;
    };
}