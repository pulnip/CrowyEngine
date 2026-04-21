#pragma once

#include <string>
#include <vector>
#include "RHIDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct ComputePass{
        std::string name;
        bool enabled = true;

        // input
        std::vector<BindSpec> inputTextures;
        std::vector<BindSpec> inputBuffers;
        std::vector<ByteBindSpec<uint32_t>> inputInts;
        // output
        std::vector<BindSpec> outputTextures;
        std::vector<BindSpec> outputBuffers;

        RHIComputePipelineStatePtr pso;
        RHISize3D gridSize;

        inline size_t numOutputs() const{
            return outputTextures.size() + outputBuffers.size();
        }
    };
}