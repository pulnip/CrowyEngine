#pragma once

#include <memory>
#include "semantics.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalPipelineState.hpp"
    #else
        #include "NullPipelineState.hpp"
    #endif
#endif

namespace Crowy
{
    // Immutable pipeline state object (graphics or compute)
    // Encapsulates shaders, rasterizer state, blend state, depth/stencil state
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIGraphicsPipelineStateType = requires(T pipelineState,
    ){
    };
    static_assert(RHIGraphicsPipelineStateType<RHIGraphicsPipelineState>);
    template<typename T>
    concept RHIComputePipelineStateType = requires(T pipelineState,
    ){
    };
    static_assert(RHIComputePipelineStateType<RHIComputePipelineState>);
#else
    class RHIGraphicsPipelineState{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIGraphicsPipelineState)
    };

    class RHIComputePipelineState{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIComputePipelineState)
    };
#endif

    using RHIGraphicsPipelineStatePtr = std::unique_ptr<RHIGraphicsPipelineState>;
    using RHIComputePipelineStatePtr = std::unique_ptr<RHIComputePipelineState>;
}
