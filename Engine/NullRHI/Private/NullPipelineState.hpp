#pragma once

#include "RHIAPI.h"
#include "RHIDefinitions.h"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
    class NullPipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    public:
        NullPipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ) : RHIPipelineState(false){}

        NullPipelineState(
            const RHIComputePipelineStateDesc& desc
        ): RHIPipelineState(true){}

        void* getNative() RHI_OVERRIDE{
            return nullptr;
        }
    };
}