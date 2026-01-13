#pragma once

#include <chrono>
#include "semantics.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Frame pacing and synchronization system
    // Manages triple buffering, frame timing, and CPU-GPU synchronization
    class FramePacer{
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

    public:
        FramePacer(RHIDevice* device) noexcept;
        ~FramePacer();

        // Begin a new frame
        // Returns true if ready to render, false if should skip
        bool beginFrame() noexcept;

        // End the current frame
        void endFrame() noexcept;

        // Wait for all frames to complete
        void waitForIdle() noexcept;

        // Get current frame index (0 to RHI_FRAMES_IN_FLIGHT-1)
        uint32_t getCurrentFrameIndex() const noexcept;
        // Get absolute frame number
        uint64_t getFrameNumber() const noexcept;
        // Get time since last frame (in seconds)
        double getDeltaTime() const noexcept;
        // Get current FPS
        double getFPS() const noexcept;
        // Get frame time in milliseconds
        double getFrameTimeMs() const noexcept;

        // Get fence for current frame
        RHIFence* getCurrentFence() noexcept;
        const RHIFence* getCurrentFence() const noexcept;

        // Get the fence value to signal for the current frame
        uint64_t getNextFenceValue() const noexcept;
    };
}
