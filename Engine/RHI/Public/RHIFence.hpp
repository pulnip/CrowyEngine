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
    protected:
        uint64_t currentValue;

    public:
        RHIFence(uint64_t initialValue = 0)
            :currentValue(initialValue){}

        DECLARE_INTERFACE(RHIFence)

        virtual void waitFor(uint64_t value) = 0;

        // Signal fence from GPU (increments to signalValue when GPU work completes)
        virtual void signal(uint64_t signalValue) = 0;

        // Wait on CPU until fence reaches waitValue
        virtual void waitCPU(uint64_t waitValue, uint64_t timeoutMs = 0) = 0;

        // Get current fence value (non-blocking query)
        virtual uint64_t getValue() = 0;

        // Check if fence has reached a value
        inline bool hasReached(uint64_t value){
            return getValue() >= value;
        }

        // Platform-specific fence getter
        virtual void* getNative() = 0;
    };
#endif
}
