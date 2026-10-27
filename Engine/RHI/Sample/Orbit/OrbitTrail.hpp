#pragma once

#include <optional>
#include <vector>

#include "Kepler.hpp"
#include "OrbitFill.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // covers Neptune's orbit, 60,225 days
    inline constexpr u32 ORBIT_TRAIL_CAPACITY = 65536;

    inline constexpr u32 ORBIT_MAX_TICKS_PER_FRAME = 64;

    inline constexpr u32 ORBIT_SAMPLE_BYTES =
        ORBIT_BODY_COUNT * static_cast<u32>(sizeof(Vec3));

    enum class OrbitFillMode : u8 { Cpu, Gpu };

    struct OrbitTrailStats {
        u32 tickCount = 0;
        u32 copyCount = 0;
        u32 dispatchCount = 0;

        u64 totalTicks = 0;
        u32 splitFrameCount = 0;
        u32 overflowFrameCount = 0;
        u64 droppedTicks = 0;
    };

    // Sample-major ring (slot s, body b at s * ORBIT_BODY_COUNT + b), so a
    // frame's new samples are one copy, two where the write wraps.
    class OrbitTrail {
    private:
        u32 capacity;
        f64 dayPerSample;

        u32 head = 0;
        u32 filled = 0;
        f64 newestDay = 0.0;
        f64 accumDays = 0.0;

        RHIBufferRAII trail;
        RHIDevice& device;

        OrbitFillMode fillMode = OrbitFillMode::Cpu;
        RAII<OrbitKeplerFill> gpuFill;

        u32 pendingSamples = 0;
        u32 pendingFirstSlot = 0;
        RHIBufferSlice pendingStaging;
        // the mode can be flipped between stage and Record
        bool pendingGpu = false;

        std::vector<Vec3> scratch;

        RHIResourceUsage resting = RHIResourceUsage::Undefined;
        RHIResourceUsage nextUse;

        OrbitTrailStats stats;

    public:
        // nextUse is fixed: a frame that pushes no samples records no barrier
        OrbitTrail(
            RHIDevice&,
            f64 dayPerSample = 1.0,
            u32 capacity = ORBIT_TRAIL_CAPACITY,
            RHIResourceUsage nextUse = RHIResourceUsage::SampledVertex
        );
        ~OrbitTrail();
        CROWY_DECLARE_PINNED(OrbitTrail)

        void Prefill(f64 endDay);

        // Pushes nothing while an earlier write waits for its Record; the
        // elapsed time is kept, so those ticks land one frame later.
        u32 Advance(f64 deltaSeconds, f64 daysPerSecond);

        void SetDayPerSample(f64);

        void SetFillMode(OrbitFillMode mode) noexcept { fillMode = mode; }
        OrbitFillMode FillMode() const noexcept { return fillMode; }

        // Call outside any pass. Empty when no samples were pushed, and then
        // the reader needs no acquire either.
        std::optional<RHIBufferBarrier> Record(RHICommandList&);

        RHIBuffer& Buffer() const noexcept { return *trail; }

        u32 Capacity() const noexcept { return capacity; }
        u32 Filled() const noexcept { return filled; }
        u32 Head() const noexcept { return head; }
        f64 DayPerSample() const noexcept { return dayPerSample; }
        f64 NewestDay() const noexcept { return newestDay; }
        const OrbitTrailStats& Stats() const noexcept { return stats; }

        // newest-first, shared with the vertex shader
        u32 SlotForAge(u32 age) const noexcept {
            CROWY_ASSERT(age < filled);

            return (head + capacity - 1 - age) % capacity;
        }
        f64 DayForAge(u32 age) const noexcept {
            CROWY_ASSERT(age < filled);

            return newestDay - age * dayPerSample;
        }

    private:
        void stage(u32 firstSlot, u32 count, f64 endDay);
    };
}
