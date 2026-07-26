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
        RHIFenceRAII fence;
        u64& frameIndex;

        RHIFrameScopeRAII scope;

    public:
        FramePacer(RHIDevice&);
        ~FramePacer();
        CROWY_DECLARE_PINNED(FramePacer)

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
