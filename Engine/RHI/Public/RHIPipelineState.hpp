#pragma once

#include "semantics.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalPipelineState.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11PipelineState.hpp"
    #else
        #include "NullPipelineState.hpp"
    #endif
#endif

namespace Crowy
{
    // Immutable pipeline state object
    // Encapsulates shaders, rasterizer state, blend state, depth/stencil state
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIGraphicsPipelineStateType = requires(T pipelineState){
        { pipelineState.getInfo() } -> std::same_as<const RHIGraphicsBindingInfo&>;
    };
    static_assert(RHIGraphicsPipelineStateType<RHIGraphicsPipelineState>);
    template<typename T>
    concept RHIComputePipelineStateType = requires(T pipelineState){
        { pipelineState.getInfo() } -> std::same_as<const RHIComputeBindingInfo&>;
    };
    static_assert(RHIComputePipelineStateType<RHIComputePipelineState>);
#else
    class RHIGraphicsPipelineState{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIGraphicsPipelineState)

        virtual const RHIGraphicsBindingInfo& getInfo() const = 0;
    };

    class RHIComputePipelineState{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIComputePipelineState)

        virtual const RHIComputeBindingInfo& getInfo() const = 0;
    };
#endif
}
