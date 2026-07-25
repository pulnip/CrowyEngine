#pragma once

#include <Metal/MTLDevice.hpp>
#include "RHIAPI.hpp"
#include "RHIFence.hpp"

namespace Crowy
{
    class MetalFence final: public RHIFence{
    private:
        MTL::SharedEvent* sharedEvent = nullptr;

    public:
        MetalFence(
            MTL::Device& device,
            u64 initialValue
        );

        ~MetalFence();

        CROWY_DECLARE_NON_COPYABLE(MetalFence)

        void WaitCPU(u64 waitValue, u64 timeoutMs) RHI_OVERRIDE;

        u64 GetValue() RHI_OVERRIDE;

        bool IsComplete(u64 value) RHI_OVERRIDE;

        MTL::SharedEvent* Get() noexcept{
            return sharedEvent;
        }
    };
}
