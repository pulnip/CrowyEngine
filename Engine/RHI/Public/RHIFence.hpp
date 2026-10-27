#pragma once

#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RHIFence {
    public:
        CROWY_DECLARE_INTERFACE(RHIFence)

        virtual void WaitCPU(u64 waitValue, u64 timeoutMs = 0) = 0;

        virtual u64 GetValue() const = 0;

        virtual bool IsComplete(u64) const = 0;
    };
}
