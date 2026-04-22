#pragma once

#include <string>
#include "RHIDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct ComputePass{
        std::string name;
        bool enabled = true;

        ShaderBindSpec cs;

        RHIComputePipelineStatePtr pso;
        RHISize3D gridSize;
    };
}