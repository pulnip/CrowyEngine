#if CROWY_BENCHMARK
#include <chrono>
#endif
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

    void FramePacer::BeginFrame(){
        scope = device.CreateFrameScope();

    #if CROWY_BENCHMARK
        lastWaitSeconds = 0.0;
    #endif

        // keep RHI_FRAMES_IN_FLIGHT batches in flight: before recording the
        // next one, wait out the oldest still outstanding
        const auto submitted = device.GetSubmittedFrame();
        if(submitted >= RHI_FRAMES_IN_FLIGHT) [[likely]] {
            const auto waitValue = submitted - RHI_FRAMES_IN_FLIGHT + 1;

        #if CROWY_BENCHMARK
            const auto before = std::chrono::steady_clock::now();
        #endif

            device.WaitFrame(waitValue);

        #if CROWY_BENCHMARK
            lastWaitSeconds = std::chrono::duration<f64>(
                std::chrono::steady_clock::now() - before
            ).count();
        #endif
        }
    }

    void FramePacer::EndFrame(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain
    ){
        device.SubmitAndPresent(cmdLists, swapchain);

        scope = nullptr;
    }

    void FramePacer::EndFrame(std::span<RHICommandList*> cmdLists){
        device.Submit(cmdLists);

        scope = nullptr;
    }

    void FramePacer::WaitForIdle(){
        device.WaitIdle();
    }
}
