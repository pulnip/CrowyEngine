#pragma once

#include "semantics.hpp"
#include "RHIFWD.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalBuffer.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIBufferType = requires(T buffer){
    };
    static_assert(RHIBufferType<RHIBuffer>);
#else
    class RHIBuffer{
    public:
        DECLARE_INTERFACE(RHIBuffer)
    };
#endif
}
