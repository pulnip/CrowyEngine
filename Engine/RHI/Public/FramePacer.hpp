#pragma once

#include <span>

#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "RHIFrameStats.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class FramePacer {
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
        f64 GetLastWaitTime() const noexcept { return lastWaitSeconds; }
#else
        constexpr f64 GetLastWaitTime() const noexcept { return 0.0; }
#endif

        // frame N begins once frame N - FramesInFlight has completed, freeing
        // slot N % FramesInFlight; returns N, counting from 1
        u64 BeginFrame();
        u64 CurrentFrame() const noexcept { return frame; }

        void EndFrame(std::span<RHICommandList*>, RHISwapchain& swapchain);

        void WaitForIdle();
    };
}
