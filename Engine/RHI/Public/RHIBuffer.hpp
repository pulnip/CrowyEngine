#pragma once

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
        RHIBuffer() = default;
        virtual ~RHIBuffer() = default;
        RHIBuffer(const RHIBuffer&) = delete;
        RHIBuffer(RHIBuffer&&) = default;
        RHIBuffer& operator=(const RHIBuffer&) = delete;
        RHIBuffer& operator=(RHIBuffer&&) = default;
    };
#endif
}
