#pragma once

#include <cstdint>
#include "semantics.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalFence.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11Fence.hpp"
    #else
        #include "NullFence.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIFenceType = requires(T fence,
    ){
    };
    static_assert(RHIFenceType<RHIFence>);
#else
    // GPU fence for CPU/GPU synchronization
    class RHIFence{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIFence)

        // Wait on CPU until fence reaches waitValue
        virtual void waitCPU(uint64_t waitValue, uint64_t timeoutMs = 0) noexcept = 0;

        // Get current fence value (non-blocking query)
        virtual uint64_t getValue() noexcept = 0;

        // Check if fence has reached a value
        virtual bool isComplete(uint64_t) noexcept = 0;
    };
#endif
}
