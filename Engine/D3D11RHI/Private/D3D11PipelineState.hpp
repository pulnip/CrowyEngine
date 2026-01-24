#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
    class D3D11PipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    public:
        D3D11PipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ){

        }

        D3D11PipelineState(
            const RHIComputePipelineStateDesc& desc
        ){
            
        }
    };
}