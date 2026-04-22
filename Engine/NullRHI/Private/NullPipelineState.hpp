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
        RHIGraphicsBindingInfo bindingInfo;

        const std::string debugName;
    
    public:
        NullGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {

        }

        const RHIGraphicsBindingInfo& getInfo() const RHI_OVERRIDE{
            return bindingInfo;
        }
    };

    class NullComputePipelineState
#ifndef USE_STATIC_RHI
        : public RHIComputePipelineState
#endif
    {
    private:
        RHIComputeBindingInfo bindingInfo;

        const std::string debugName;
    
    public:
        NullComputePipelineState(
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        )
            : debugName(name)
        {

        }

        const RHIComputeBindingInfo& getInfo() const RHI_OVERRIDE{
            return bindingInfo;
        }
    };
}