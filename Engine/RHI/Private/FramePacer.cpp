#include "Assert.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "RHIFrameScope.hpp"
#include "FramePacer.hpp"

namespace Crowy
{
    class FramePacer::Impl{
    private:
        RHIFrameScopeRAII scope = nullptr;
        RHIDevice& device;
        RHIFenceRAII fence;
        u64& frameIndex;

    public:
        Impl(RHIDevice& device)
            : device(device)
            , frameIndex(device.GetFrameIndexRef())
            , fence(device.CreateFence(0))
        {
            CROWY_ASSERT(fence != nullptr);
        }

        ~Impl() = default;

        bool BeginFrame(){
            scope = device.CreateFrameScope();
            if(frameIndex < RHI_FRAMES_IN_FLIGHT) [[unlikely]]
                return true;

            auto waitValue = frameIndex - RHI_FRAMES_IN_FLIGHT + 1;
            fence->WaitCPU(waitValue);

            return true;
        }

        void EndFrame(){
            device.SignalFence(*fence, ++frameIndex);
            scope = nullptr;
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

    FramePacer::FramePacer(RHIDevice& device)
        : impl(std::make_unique<Impl>(device)){}

    FramePacer::~FramePacer() = default;

    bool FramePacer::BeginFrame(){
        return impl->BeginFrame();
    }

    void FramePacer::EndFrame(){
        impl->EndFrame();
    }

    void FramePacer::WaitForIdle(){
        impl->WaitForIdle();
    }
}
