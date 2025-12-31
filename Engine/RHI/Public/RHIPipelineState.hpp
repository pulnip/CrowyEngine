#pragma once

#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

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
    concept RHIPipelineStateType = requires(T pipelineState,
    ){
    };
    static_assert(RHIPipelineStateType<RHIPipelineState>);
#else
    class RHIPipelineState{
    public:
        DECLARE_INTERFACE(RHIPipelineState)
    };
#endif
}
