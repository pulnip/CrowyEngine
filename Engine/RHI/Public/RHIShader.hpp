#pragma once

#include "semantics.hpp"
#include "RHIFWD.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalShader.hpp"
    #else
        #include "NullShader.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIShaderType = requires(T shader){
    };
    static_assert(RHIShaderType<RHIShader>);
#else
    class RHIShader{
    public:
        DECLARE_INTERFACE(RHIShader)
    };
#endif
}
