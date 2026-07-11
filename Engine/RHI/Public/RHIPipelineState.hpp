#pragma once

#include "Semantics.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class RHIGraphicsPipelineState{
    public:
        SMOL_DECLARE_INTERFACE(RHIGraphicsPipelineState)

        virtual const RHIGraphicsBindingInfo& GetInfo() const = 0;
    };

    class RHIComputePipelineState{
    public:
        SMOL_DECLARE_INTERFACE(RHIComputePipelineState)

        virtual const RHIComputeBindingInfo& GetInfo() const = 0;
    };
}
