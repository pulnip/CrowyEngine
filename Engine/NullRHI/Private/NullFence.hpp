#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIFence.hpp"
#endif

namespace Crowy
{
    class NullFence
#ifndef USE_STATIC_RHI
        : public RHIFence
#endif
    {
    public:
        NullFence(
            uint64_t initialValue
        ){}

        void waitCPU(uint64_t waitValue, uint64_t timeoutMs) noexcept RHI_OVERRIDE{

        }

        uint64_t getValue() noexcept RHI_OVERRIDE{
            return 0;
        }

        bool isComplete(uint64_t value) noexcept RHI_OVERRIDE{
            return true;
        }
    };
}