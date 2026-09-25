#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include "FrameHistory.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // how many frames of GPU time a device keeps once they are read back
    inline constexpr usize GPUTimeHistoryDepth = 8;

    using RHIGPUTimeHistory = FrameHistory<f64, GPUTimeHistoryDepth>;

    // one command list's run on the GPU clock, in seconds
    struct RHIGPUInterval {
        f64 begin = 0.0;
        f64 end = 0.0;
    };

    // how long the GPU was busy with any of them: the length of their union,
    // so a gap where the GPU starved between lists is not GPU time. An
    // interval that ends before it begins counts as empty
    constexpr f64 busySeconds(std::span<const RHIGPUInterval> intervals) {
        std::vector<RHIGPUInterval> sorted(intervals.begin(), intervals.end());
        std::ranges::sort(sorted, {}, &RHIGPUInterval::begin);

        f64 busy = 0.0;
        bool open = false;
        RHIGPUInterval run;
        for(const auto& interval: sorted) {
            if(interval.end <= interval.begin)
                continue;

            if(open && interval.begin <= run.end) {
                run.end = std::max(run.end, interval.end);
                continue;
            }

            if(open)
                busy += run.end - run.begin;
            run = interval;
            open = true;
        }
        if(open)
            busy += run.end - run.begin;

        return busy;
    }

    namespace GPUTimePins
    {
        using Two = std::array<RHIGPUInterval, 2>;

        static_assert(busySeconds({}) == 0.0);
        static_assert(
            busySeconds(Two{{{0.0, 1.0}, {0.5, 2.0}}}) == 2.0,
            "overlapping"
        );
        static_assert(
            busySeconds(Two{{{0.0, 1.0}, {1.5, 2.0}}}) == 1.5,
            "disjoint"
        );
        static_assert(
            busySeconds(Two{{{0.0, 4.0}, {1.0, 2.0}}}) == 4.0,
            "nested"
        );
        static_assert(
            busySeconds(Two{{{1.5, 2.0}, {0.0, 1.0}}}) == 1.5,
            "out of order"
        );
        static_assert(
            busySeconds(Two{{{2.0, 1.0}, {0.0, 0.25}}}) == 0.25,
            "reversed"
        );
    }
}
