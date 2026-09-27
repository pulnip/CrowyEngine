#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct RHIGraphicsPipelineStateDesc;

    // throws where a desc breaks a rule every backend shares
    void ValidateGraphicsPipelineDesc(const RHIGraphicsPipelineStateDesc& desc);

    class RHIGraphicsPipelineState{
    public:
        CROWY_DECLARE_INTERFACE(RHIGraphicsPipelineState)
    };

    class RHIComputePipelineState{
    public:
        CROWY_DECLARE_INTERFACE(RHIComputePipelineState)
    };
}
