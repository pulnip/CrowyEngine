#include "Assert.hpp"
#include "FramePacer.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "RHIFrameScope.hpp"

namespace Crowy
{
    FramePacer::FramePacer(
        RHIDevice& device
    )
        : device(device)
        , frameIndex(device.GetFrameIndexRef())
        , fence(device.CreateFence(0))
    {
        CROWY_ASSERT(fence != nullptr);
    }

    FramePacer::~FramePacer() = default;

    void FramePacer::BeginFrame(){
        scope = device.CreateFrameScope();

        if(frameIndex >= RHI_FRAMES_IN_FLIGHT) [[likely]] {
            auto waitValue = frameIndex - RHI_FRAMES_IN_FLIGHT + 1;
            fence->WaitCPU(waitValue);
        }
    }

    void FramePacer::EndFrame(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain
    ){
        device.SubmitAndPresent(cmdLists, swapchain, *fence);

        scope = nullptr;
    }

    void FramePacer::EndFrame(std::span<RHICommandList*> cmdLists){
        device.Submit(cmdLists, *fence);

        scope = nullptr;
    }

    void FramePacer::WaitForIdle(){
        // Signal fresh so this also waits on any GPU work queued
        // after the last per-frame signal (e.g. swapchain Present).
        device.SignalFence(*fence, ++frameIndex);
        fence->WaitCPU(frameIndex);
    }
}
