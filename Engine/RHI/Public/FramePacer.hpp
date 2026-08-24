#pragma once

#include <span>
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Frame pacing and synchronization system
    // Manages triple buffering, frame timing, and CPU-GPU synchronization
    class FramePacer{
    private:
        RHIDevice& device;
        u64& frameIndex;

        RHIFrameScopeRAII scope;

    #if CROWY_BENCHMARK
        f64 lastWaitSeconds = 0.0;
    #endif

    public:
        FramePacer(RHIDevice&);
        ~FramePacer();
        CROWY_DECLARE_PINNED(FramePacer)

        // How long the last BeginFrame() blocked on the GPU. Large means the
        // GPU is the bottleneck; near zero means the CPU is.
    #if CROWY_BENCHMARK
        f64 GetLastWaitTime() const noexcept{ return lastWaitSeconds; }
    #else
        constexpr f64 GetLastWaitTime() const noexcept{ return 0.0; }
    #endif

        // Begin a new frame
        void BeginFrame();

        // End the current frame for rendering
        void EndFrame(
            std::span<RHICommandList*>,
            RHISwapchain& swapchain
        );
        // End the current frame for computing only
        void EndFrame(
            std::span<RHICommandList*>
        );

        // Wait for all frames to complete
        void WaitForIdle();
    };
}
