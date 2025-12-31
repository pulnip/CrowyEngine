#pragma once

#include <Metal/Metal.hpp>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
    class MetalPipelineState
#ifndef USE_STATIC_RHI
        : public RHIPipelineState
#endif
    {
    public:
        MetalPipelineState(
            MTL::Device* device,
            const RHIGraphicsPipelineStateDesc& desc
        ) : RHIPipelineState(false){}

        MetalPipelineState(
            MTL::Device* device,
            const RHIComputePipelineStateDesc& desc
        ): RHIPipelineState(true){}

        void* getNative() RHI_OVERRIDE{
            return nullptr;
        }
    };
}