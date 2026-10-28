#pragma once

#include <Metal/MTLCommandBuffer.hpp>
#include <Metal/MTLDevice.hpp>

#include "MetalDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RHIFence.hpp"

namespace Crowy
{
    class MetalFence final: public RHIFence {
    private:
        RefPtr<MTL::SharedEvent> sharedEvent;

    public:
        MetalFence(MTL::Device& device, u64 initialValue);

        ~MetalFence();

        CROWY_DECLARE_NON_COPYABLE(MetalFence)

        void WaitCPU(u64 waitValue, u64 timeoutMs) override;

        u64 GetValue() const override;

        bool IsComplete(u64 value) const override;

        void Encode(MTL::CommandBuffer&, u64 value);
    };
}
