#pragma once

#include <cstdint>
#include "semantics.hpp"
#include "RHIFWD.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalFence.hpp"
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
        DECLARE_INTERFACE(RHIFence)

        // Wait on CPU until fence reaches waitValue
        virtual void waitCPU(uint64_t waitValue, uint64_t timeoutMs = 0) = 0;

        // Get current fence value (non-blocking query)
        virtual uint64_t getValue() = 0;

        // Check if fence has reached a value
        virtual bool isComplete(uint64_t) = 0;
    };
#endif
}
