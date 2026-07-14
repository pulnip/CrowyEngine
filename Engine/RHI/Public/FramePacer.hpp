#pragma once

#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RHIDevice;
    // Frame pacing and synchronization system
    // Manages triple buffering, frame timing, and CPU-GPU synchronization
    class FramePacer{
    private:
        class Impl;
        RAII<Impl> impl;

    public:
        FramePacer(RHIDevice& device);
        ~FramePacer();
        SMOL_DECLARE_MOVE_ONLY(FramePacer)

        // Begin a new frame
        // Returns true if ready to render, false if should skip
        bool BeginFrame();

        // End the current frame
        void EndFrame();

        // Wait for all frames to complete
        void WaitForIdle();
    };
}
