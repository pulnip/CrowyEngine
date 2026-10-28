#pragma once

#include <functional>
#include <span>
#include <vector>

#include "CommandPort.hpp"
#include "FrameHistory.hpp"
#include "FrameGraph.hpp"
#include "FrameProfiler.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // one finished frame: what the loop measured and what the renderer
    // counted, taken together so every reader sees the same frame
    struct FrameStats {
        FrameReport report;
        usize primitives = 0;
        u32 visiblePrimitives = 0;
        u64 triangles = 0;
        u32 draws = 0;
        usize runs = 0;
        usize pipelines = 0;
        // in list order; draws, runs and triangles sum these
        std::vector<PassStats> passes;
    };

    // The frames that ended, as read_stats and the overlays read them.
    class FrameStatsLog {
    public:
        using LastFrame = std::function<u64()>;

        // how far back read_stats can name a frame
        static constexpr usize Depth = 64;

    private:
        using History = FrameHistory<FrameStats, Depth>;

        History history;
        // this frame's counts, summed over its passes; End adds the report
        // and the totals
        FrameStats recorded;

    public:
        FrameStatsLog() = default;
        ~FrameStatsLog() = default;
        // the port's verbs point at it
        CROWY_DECLARE_PINNED(FrameStatsLog)

        // before the frame records, with what its main view let through
        void Begin(u32 visiblePrimitives);
        // after it recorded; the edges and a hook pass's draws come counted
        void AddPasses(std::span<const PassStats> passes);
        // keyed by the loop's own frame number, which every build keeps
        void End(
            u64 frame,
            const FrameReport& report,
            usize primitives,
            usize pipelines
        );

        // with no selector the newest frame that ended; with one, each frame
        // it names, from the history
        void RegisterVerbs(CommandPort& port, LastFrame lastFrame);

        const FrameStats& Recorded() const noexcept { return recorded; }
        // the newest frame that ended; empty before frame 1 has
        const FrameStats& Last() const noexcept;

    private:
        // the history's entry for the frame stats.report.gpu times, if kept
        const FrameStats* gpuFrameOf(const FrameStats& stats) const noexcept;
    };
}
