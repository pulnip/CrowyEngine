#pragma once

#include <Metal/Metal.hpp>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
#ifndef USE_STATIC_RHI
    #include "RHIFence.hpp"
#endif

namespace Crowy
{
    class MetalFence
#ifndef USE_STATIC_RHI
        : public RHIFence
#endif
    {
    public:
        MetalFence(
            MTL::Device* device,
            uint64_t initialValue
        ): RHIFence(initialValue){}

        void waitFor(uint64_t value) RHI_OVERRIDE{

        }

        void signal(uint64_t signalValue) RHI_OVERRIDE{

        }

        void waitCPU(uint64_t waitValue, uint64_t timeoutMs) RHI_OVERRIDE{

        }

        uint64_t getValue() RHI_OVERRIDE{

        }

        void* getNative() RHI_OVERRIDE{
            return nullptr;
        }
    };
}