#pragma once

#include <d3d11.h>
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
            ID3D11Device* device,
            const RHIGraphicsPipelineStateDesc& desc
        ){

        }

        D3D11PipelineState(
            ID3D11Device* device,
            const RHIComputePipelineStateDesc& desc
        ){

        }
    };
}