#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIPipelineState.hpp"
#endif

namespace Crowy
{
    class NullGraphicsPipelineState
#ifndef USE_STATIC_RHI
        : public RHIGraphicsPipelineState
#endif
    {
    private:
        const std::string debugName;
    
    public:
        NullGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {

        }
    };

    class NullComputePipelineState
#ifndef USE_STATIC_RHI
        : public RHIComputePipelineState
#endif
    {
    private:
        const std::string debugName;
    
    public:
        NullComputePipelineState(
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {

        }
    };
}