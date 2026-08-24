#pragma once

#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // GPU fence for CPU/GPU synchronization
    class RHIFence{
    public:
        CROWY_DECLARE_INTERFACE(RHIFence)

        // Wait on CPU until fence reaches waitValue
        virtual void WaitCPU(u64 waitValue, u64 timeoutMs = 0) = 0;

        // Get current fence value (non-blocking query)
        virtual u64 GetValue() const = 0;

        // Check if fence has reached a value
        virtual bool IsComplete(u64) const = 0;
    };
}
