#pragma once

#include <optional>
#include <vector>
#include "Kepler.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // 65536 samples at one day each cover Neptune's 165-year orbit
    // (60,225 samples) with room to spare. 65536 * 9 * 12 B = 7 MB.
    inline constexpr u32 ORBIT_TRAIL_CAPACITY = 65536;

    // Ticks past this in one frame are dropped: sim time slows down but the
    // frame survives. Dropped ticks are counted, never silent.
    inline constexpr u32 ORBIT_MAX_TICKS_PER_FRAME = 64;

    // one ring slot: every body at one instant, sample-major
    inline constexpr u32 ORBIT_SAMPLE_BYTES =
        ORBIT_BODY_COUNT * static_cast<u32>(sizeof(Vec3));

    struct OrbitTrailStats{
        // this frame
        u32 tickCount = 0;
        // 0 when idle, 1 normally, 2 when the write straddles the ring seam
        u32 copyCount = 0;

        // cumulative
        u64 totalTicks = 0;
        u32 splitFrameCount = 0;
        // frames that hit ORBIT_MAX_TICKS_PER_FRAME and dropped the remainder
        u32 overflowFrameCount = 0;
        u64 droppedTicks = 0;
    };

    // Sample-major ring of heliocentric positions, living in a GPU-only buffer.
    //
    //     slot s, body b  ->  element s * ORBIT_BODY_COUNT + b
    //
    // Sample-major is what makes a tick one contiguous run of
    // ORBIT_SAMPLE_BYTES, so a frame's new samples reach the GPU in a single
    // copy - two only where the write wraps past the end of the ring.
    //
    // The ring has to survive between frames, which rules out a CPUWrite
    // buffer: those are multiplexed across RHI_FRAMES_IN_FLIGHT physical slots,
    // so an incremental append would land in one slot out of three and the
    // other two would hold stale samples. Hence GPU-only storage plus an
    // explicit staging copy. Step 7 replaces the staging path with a compute
    // fill and the ring itself does not change.
    class OrbitTrail{
    private:
        u32 capacity;
        f64 dayPerSample;

        // where the next sample goes; the newest one sits at head - 1
        u32 head = 0;
        u32 filled = 0;
        // sim day of the newest stored sample
        f64 newestDay = 0.0;
        f64 accumDays = 0.0;

        RHIBufferRAII trail;
        // RHI_FRAMES_IN_FLIGHT regions of one full ring each.
        //
        // A full ring because a prefill writes every slot, and one region per
        // frame in flight because a CPUWrite buffer declared CopySrc is a
        // single physical allocation - the RHI only multiplexes buffers that
        // are not pure copy sources, on the grounds that a copy source needs
        // fence-aware suballocation instead (that is what UploadRing does with
        // the device's own staging). Writing one region while another frame's
        // copy is still reading its own is the whole point.
        //
        // Costs three rings' worth of upload heap, which Step 7 deletes when
        // compute takes over the fill.
        RHIBufferRAII staging;
        const u64& frameIndex;

        // written into `staging` by Advance/Prefill, copied by Record
        u32 pendingSamples = 0;
        u32 pendingFirstSlot = 0;
        u64 pendingStagingOffset = 0;

        std::vector<Vec3> scratch;

        // what the previous submission left the trail in; Undefined until the
        // first Record
        RHIResourceUsage resting = RHIResourceUsage::Undefined;
        RHIResourceUsage nextUse;

        OrbitTrailStats stats;

    public:
        // `nextUse` is what reads the trail after every Record - the vertex
        // stage in the sample, a copy in the headless check. It is fixed for
        // the lifetime of the trail because a frame that pushes no samples
        // records no barrier at all, and a reader that changed its mind would
        // have nothing to pair with.
        OrbitTrail(
            RHIDevice&,
            f64 dayPerSample = 1.0,
            u32 capacity = ORBIT_TRAIL_CAPACITY,
            RHIResourceUsage nextUse = RHIResourceUsage::SampledVertex
        );
        ~OrbitTrail();
        CROWY_DECLARE_PINNED(OrbitTrail)

        // Fills the whole ring with the `capacity` samples ending at `endDay`.
        // Without this the trail draws itself in over real minutes - Neptune's
        // orbit is 60,225 ticks - so it runs at startup, on Reset, and whenever
        // dayPerSample changes and makes the stored spacing meaningless.
        void Prefill(f64 endDay);

        // Fixed sim timestep, so trail length in days does not follow the frame
        // rate. Returns the number of ticks pushed.
        //
        // Pushes nothing while an earlier write is still waiting for its
        // Record - the staging region holds one frame's samples, and the first
        // frame's is the prefill. The elapsed time is kept either way, so the
        // only effect is that those ticks land one frame later.
        u32 Advance(f64 deltaSeconds, f64 daysPerSecond);

        // dayPerSample only means something together with the samples already
        // stored, so changing it refills the ring.
        void SetDayPerSample(f64);

        // Records this frame's staging -> ring copies. Call outside any pass.
        // Returns the release half of the copy, which whatever reads the trail
        // must acquire with; empty when no samples were pushed, in which case
        // the reader needs no acquire either - nothing wrote the buffer.
        std::optional<RHIBufferBarrier> Record(RHICommandList&);

        RHIBuffer& Buffer() const noexcept{ return *trail; }

        u32 Capacity() const noexcept{ return capacity; }
        u32 Filled() const noexcept{ return filled; }
        u32 Head() const noexcept{ return head; }
        f64 DayPerSample() const noexcept{ return dayPerSample; }
        f64 NewestDay() const noexcept{ return newestDay; }
        const OrbitTrailStats& Stats() const noexcept{ return stats; }

        // Newest-first addressing, shared with the vertex shader: age 0 is the
        // sample just pushed. Trails are drawn from the head backwards, so this
        // is the only indexing either side needs - and the ring seam it never
        // reaches cannot produce a stray segment.
        u32 SlotForAge(u32 age) const noexcept{
            CROWY_ASSERT(age < filled);

            return (head + capacity - 1 - age) % capacity;
        }
        f64 DayForAge(u32 age) const noexcept{
            CROWY_ASSERT(age < filled);

            return newestDay - age * dayPerSample;
        }

    private:
        // generates `count` samples ending at `endDay` into `scratch` and
        // stages them for the slots starting at `firstSlot`
        void stage(u32 firstSlot, u32 count, f64 endDay);
    };
}
