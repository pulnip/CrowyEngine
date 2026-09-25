#pragma once

#include <span>
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIFrameStats.hpp"

namespace Crowy
{
    // Frame pacing and synchronization system
    // Manages triple buffering, frame timing, and CPU-GPU synchronization
    class FramePacer{
    private:
        RHIDevice& device;

        RHIFrameScopeRAII scope;
        // the frame being recorded; between frames, the last one that began
        u64 frame = 0;

    #if CROWY_FRAME_STATS
        f64 lastWaitSeconds = 0.0;
    #endif

    public:
        FramePacer(RHIDevice&);
        ~FramePacer();
        CROWY_DECLARE_PINNED(FramePacer)

        // How long the last BeginFrame() blocked on the GPU. Large means the
        // GPU is the bottleneck; near zero means the CPU is.
    #if CROWY_FRAME_STATS
        f64 GetLastWaitTime() const noexcept{ return lastWaitSeconds; }
    #else
        constexpr f64 GetLastWaitTime() const noexcept{ return 0.0; }
    #endif

        // Frame N begins only after frame N - RHI_FRAMES_IN_FLIGHT has
        // completed, so its per-frame slot N % RHI_FRAMES_IN_FLIGHT is free.
        // Returns N; frames count from 1.
        u64 BeginFrame();
        u64 CurrentFrame() const noexcept{ return frame; }

        // End the current frame for rendering
        void EndFrame(
            std::span<RHICommandList*>,
            RHISwapchain& swapchain
        );

        // Wait for all frames to complete
        void WaitForIdle();
    };
}
