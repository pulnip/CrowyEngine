#include "Assert.hpp"
#include "DX12Device.hpp"
#include "RHIFence.hpp"
// #include "RHIFrameScope.hpp"
#include "DX12FramePacer.hpp"

namespace Crowy
{
    class DX12FramePacer::Impl{
    private:
        // RHIFrameScopeRAII scope = nullptr;
        DX12Device& device;
        RHIFenceRAII fence;
        u64& frameIndex;

    public:
        Impl(DX12Device& device)
            : device(device)
            , frameIndex(device.GetFrameIndexRef())
            , fence(device.CreateFence(0))
        {
            SMOL_ASSERT(fence != nullptr);
        }

        ~Impl() = default;

        bool BeginFrame(){
            // scope = device.CreateFrameScope();
            if(frameIndex < RHI_FRAMES_IN_FLIGHT) [[unlikely]]
                return true;

            auto waitValue = frameIndex - RHI_FRAMES_IN_FLIGHT + 1;
            fence->WaitCPU(waitValue);

            return true;
        }

        void EndFrame(){
            device.SignalFence(*fence, ++frameIndex);
            // scope = nullptr;
        }

        void WaitForIdle(){
            if(frameIndex == 0) [[unlikely]]
                return;

            // Signal fresh so this also waits on any GPU work queued
            // after the last per-frame signal (e.g. swapchain Present).
            device.SignalFence(*fence, ++frameIndex);
            fence->WaitCPU(frameIndex);
        }

        auto& GetCurrentFence(this auto& self) noexcept{
            return *self.fence.get();
        }

        u64 GetNextFenceValue() const noexcept{
            return frameIndex + 1;
        }
    };

    DX12FramePacer::DX12FramePacer(DX12Device& device)
        : impl(std::make_unique<Impl>(device)){}

    DX12FramePacer::~DX12FramePacer() = default;

    bool DX12FramePacer::BeginFrame(){
        return impl->BeginFrame();
    }

    void DX12FramePacer::EndFrame(){
        impl->EndFrame();
    }

    void DX12FramePacer::WaitForIdle(){
        impl->WaitForIdle();
    }
}
