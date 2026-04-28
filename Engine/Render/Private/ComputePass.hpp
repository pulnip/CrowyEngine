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

        RHIComputePipelineStateRAII pso;
        RHISize3D gridSize;
    };
}