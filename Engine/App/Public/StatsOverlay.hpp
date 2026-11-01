#pragma once

#include <array>

#include "FrameProfiler.hpp"
#include "FrameStatsLog.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    class StatsOverlay {
    private:
        std::array<f64, NUM_FRAME_SECTION> averageMs{};
        u64 lastFrame = 0;
        // the GPU runs behind, so its row moves on its own frames
        f64 averageGPUMs = 0.0;
        u64 lastGPUFrame = 0;

    public:
        void Draw(const FrameStats& stats);
    };
}
