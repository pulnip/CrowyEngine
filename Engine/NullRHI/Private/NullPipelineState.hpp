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
    private:
        const std::string debugName;
    
    public:
        NullPipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {

        }

        NullPipelineState(
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {

        }
    };
}