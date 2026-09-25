#include <chrono>
#include "FramePacer.hpp"
#include "RHIDevice.hpp"
#include "RHIFrameScope.hpp"

namespace Crowy
{
    FramePacer::FramePacer(
        RHIDevice& device
    )
        : device(device){}

    FramePacer::~FramePacer() = default;

    u64 FramePacer::BeginFrame(){
        scope = device.CreateFrameScope();
        ++frame;

    #if CROWY_FRAME_STATS
        lastWaitSeconds = 0.0;
    #endif

        if(frame > RHI_FRAMES_IN_FLIGHT) [[likely]] {
        #if CROWY_FRAME_STATS
            const auto before = std::chrono::steady_clock::now();
        #endif

            device.WaitFrame(frame - RHI_FRAMES_IN_FLIGHT);

        #if CROWY_FRAME_STATS
            lastWaitSeconds = std::chrono::duration<f64>(
                std::chrono::steady_clock::now() - before
            ).count();
        #endif
        }

        return frame;
    }

    void FramePacer::EndFrame(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain
    ){
        device.SubmitAndPresent(cmdLists, swapchain, frame);

        scope = nullptr;
    }

    void FramePacer::WaitForIdle(){
        device.WaitIdle();
    }
}
