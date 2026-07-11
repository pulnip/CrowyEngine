#pragma once

#include "DX12Definitions.hpp"
#include "RHIAPI.hpp"
#include "RHIFence.hpp"

namespace Crowy
{
    class DX12Fence: public RHIFence{
    private:
        FenceRAII fence = nullptr;
        HANDLE fenceEvent = nullptr;

    public:
        DX12Fence(
            Device& device,
            u64 initialValue
        );

        ~DX12Fence();

        void WaitCPU(u64 waitValue, u64 timeoutMs) RHI_OVERRIDE;

        u64 GetValue() RHI_OVERRIDE;

        bool IsComplete(u64 value) RHI_OVERRIDE;

        Fence* Get() noexcept{ return fence.Get(); }
    };
}
