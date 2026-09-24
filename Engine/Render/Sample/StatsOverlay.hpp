#pragma once

#include <array>

#include "FrameProfiler.hpp"
#include "Primitives.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    // The `stat unit` corner: what the last finished frame cost and what it
    // asked for. CPU times are smoothed across frames; counts are raw.
    class StatsOverlay {
    private:
        std::array<f64, NUM_FRAME_SECTION> averageMs{};
        u64 lastFrame = 0;

    public:
        void Draw(const RenderApp::FrameStats& stats);
    };
}
