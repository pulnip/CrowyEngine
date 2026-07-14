#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RHIGraphicsPipelineState{
    public:
        SMOL_DECLARE_INTERFACE(RHIGraphicsPipelineState)
    };

    class RHIComputePipelineState{
    public:
        SMOL_DECLARE_INTERFACE(RHIComputePipelineState)
    };
}
