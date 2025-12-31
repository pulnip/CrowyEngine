#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
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
        ){

        }

        NullPipelineState(
            const RHIComputePipelineStateDesc& desc
        ){
            
        }
    };
}