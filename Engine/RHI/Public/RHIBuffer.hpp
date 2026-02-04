#pragma once

#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalBuffer.hpp"
    #else
        #include "NullBuffer.hpp"
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
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIBuffer)

        virtual void upload(
            const void* data, size_t size,
            size_t offset = 0
        ) noexcept = 0;

        virtual RHIResourceState getState() const noexcept = 0;
        virtual void setState(RHIResourceState state) noexcept = 0;
    };
#endif
}
