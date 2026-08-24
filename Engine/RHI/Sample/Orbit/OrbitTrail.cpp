#include <algorithm>
#include <array>
#include <span>
#include "EnumUtil.hpp"
#include "OrbitTrail.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    OrbitTrail::OrbitTrail(
        RHIDevice& device,
        f64 dayPerSample,
        u32 capacity,
        RHIResourceUsage nextUse
    )
        : capacity(capacity)
        , dayPerSample(dayPerSample)
        , device(device)
        , nextUse(nextUse)
    {
        CROWY_ASSERT(capacity > 1, "a trail needs two samples to have a segment");
        CROWY_ASSERT(dayPerSample > 0.0);
        // buffer sizes are u32; 7 MB of ring at the default capacity
        CROWY_ASSERT(capacity <= 0xFFFFFFFFu / ORBIT_SAMPLE_BYTES);
        CROWY_ASSERT(capacity <= ORBIT_PHASE_MAX_STEPS,
            "a prefill walks the fixed-point phase over the whole ring, and "
            "the block split only reaches ORBIT_PHASE_MAX_STEPS"
        );

        const auto ringBytes = capacity * ORBIT_SAMPLE_BYTES;

        trail = device.CreateBuffer(RHIBufferCreateDesc{
            .size = ringBytes,
            .usage = combine(
                // the trail is pulled by SV_VertexID/SV_InstanceID, never bound
                // as a vertex buffer
                RHIBufferUsage::ShaderResource,
                // the compute fill writes it through a UAV
                RHIBufferUsage::UnorderedAccess,
                RHIBufferUsage::CopyDst,
                // the headless check reads the ring back; harmless otherwise
                RHIBufferUsage::CopySrc
            ),
            .location = RHIMemoryLocation::Device
        }, "OrbitTrailRing");

        gpuFill = std::make_unique<OrbitKeplerFill>(device, ORBIT_ELEMENTS);

        scratch.reserve(static_cast<usize>(capacity) * ORBIT_BODY_COUNT);
    }

    OrbitTrail::~OrbitTrail() = default;

    void OrbitTrail::Prefill(f64 endDay){
        head = 0;
        filled = capacity;
        newestDay = endDay;
        accumDays = 0.0;
        pendingSamples = 0;

        // head 0 means the whole ring is one contiguous run starting at slot 0,
        // so even the biggest write this class ever does is a single copy
        stage(0, capacity, endDay);
    }

    u32 OrbitTrail::Advance(f64 deltaSeconds, f64 daysPerSecond){
        CROWY_ASSERT(daysPerSecond >= 0.0, "sim time does not run backwards");

        stats.tickCount = 0;
        stats.copyCount = 0;
        accumDays += deltaSeconds * daysPerSecond;

        // an unrecorded write is still sitting in this frame's staging region -
        // the prefill on the first frame, normally. The elapsed time is not
        // lost, it just ticks once the copy has been recorded.
        if(pendingSamples != 0)
            return 0;

        // a tiny ring would otherwise let one frame lap itself, and the copy
        // below assumes the write wraps at most once
        const auto maxTicks = std::min(ORBIT_MAX_TICKS_PER_FRAME, capacity);

        u32 ticks = 0;
        while(accumDays >= dayPerSample && ticks < maxTicks){
            accumDays -= dayPerSample;
            ++ticks;
        }

        if(accumDays >= dayPerSample){
            // the remainder is dropped rather than carried: carrying it would
            // just push the same overrun into the next frame. Sim time slows
            // down, which the counters make visible instead of mysterious.
            stats.droppedTicks += static_cast<u64>(accumDays / dayPerSample);
            ++stats.overflowFrameCount;
            accumDays = 0.0;
        }

        if(ticks == 0)
            return 0;

        const auto firstSlot = head;

        newestDay += ticks * dayPerSample;
        head = (head + ticks) % capacity;
        filled = std::min(filled + ticks, capacity);

        stage(firstSlot, ticks, newestDay);

        stats.tickCount = ticks;
        stats.totalTicks += ticks;

        return ticks;
    }

    void OrbitTrail::SetDayPerSample(f64 value){
        CROWY_ASSERT(value > 0.0);

        if(value == dayPerSample)
            return;

        // the stored samples are spaced by the old value, so keeping them would
        // mean a trail whose segments span two different amounts of sim time
        dayPerSample = value;
        Prefill(newestDay);
    }

    std::optional<RHIBufferBarrier> OrbitTrail::Record(RHICommandList& cmdList){
        stats.copyCount = 0;
        stats.dispatchCount = 0;

        if(pendingSamples == 0)
            return std::nullopt;

        const auto firstRun = std::min(pendingSamples, capacity - pendingFirstSlot);
        const auto secondRun = pendingSamples - firstRun;

        // the two paths differ only in how the ring is written; the edge either
        // way carries it from whatever the last submission left behind to
        // whatever reads it next
        const auto writeAs = pendingGpu ?
            RHIResourceUsage::StorageCompute :
            RHIResourceUsage::CopyDst;

        // the ring keeps its older samples, so this is a partial write and the
        // acquire must not discard
        const auto acquire = resting == RHIResourceUsage::Undefined ?
            MakeBarrier(*trail,
                RHIResourceUsage::Undefined,
                writeAs
            ) :
            MakeCrossSubmissionBarrier(*trail,
                resting,
                writeAs
            );
        const auto release = MakeBarrier(*trail, writeAs, nextUse);

        const std::array acquires{acquire};
        const std::array releases{release};

        if(pendingGpu){
            cmdList.BeginComputePass({}, acquires);

            // one thread per (sample, body); the wrap is the shader's modulo,
            // so unlike the copy path there is no second run to issue
            gpuFill->RecordRing(
                cmdList,
                *trail,
                pendingFirstSlot,
                pendingSamples,
                capacity
            );

            cmdList.EndComputePass({}, releases);

            stats.dispatchCount = 1;
            if(secondRun > 0)
                ++stats.splitFrameCount;

            resting = nextUse;
            pendingSamples = 0;

            return release;
        }

        cmdList.BeginBlitPass({}, acquires);

        cmdList.Copy(
            *pendingStaging.buffer,
            *trail,
            pendingStaging.offset,
            static_cast<usize>(pendingFirstSlot) * ORBIT_SAMPLE_BYTES,
            static_cast<usize>(firstRun) * ORBIT_SAMPLE_BYTES
        );
        if(secondRun > 0){
            // sample-major pays off here: even the wrapped case is two runs,
            // not one copy per body
            cmdList.Copy(
                *pendingStaging.buffer,
                *trail,
                pendingStaging.offset +
                    static_cast<usize>(firstRun) * ORBIT_SAMPLE_BYTES,
                0,
                static_cast<usize>(secondRun) * ORBIT_SAMPLE_BYTES
            );
        }

        cmdList.EndBlitPass({}, releases);

        stats.copyCount = secondRun > 0 ? 2 : 1;
        if(secondRun > 0)
            ++stats.splitFrameCount;

        resting = nextUse;
        pendingSamples = 0;

        return release;
    }

    void OrbitTrail::stage(u32 firstSlot, u32 count, f64 endDay){
        CROWY_ASSERT(pendingSamples == 0);
        CROWY_ASSERT(0 < count && count <= capacity);
        CROWY_ASSERT(firstSlot < capacity);

        pendingFirstSlot = firstSlot;
        pendingSamples = count;
        pendingGpu = fillMode == OrbitFillMode::Gpu;

        if(pendingGpu){
            // nothing to generate here: the epoch is the whole handoff, and
            // Record turns it into one dispatch. Sample 0 of the run is the
            // oldest, so the epoch counts back from endDay the same way the
            // CPU path's day expression does.
            gpuFill->SetEpoch(
                endDay - (count - 1) * dayPerSample,
                dayPerSample
            );

            return;
        }

        scratch.resize(static_cast<usize>(count) * ORBIT_BODY_COUNT);

        for(u32 s=0; s<count; ++s){
            // counted back from endDay, not forward from the start, so this is
            // the same expression DayForAge uses - the newest batch then
            // reproduces bit-exactly and a readback check can demand equality
            // rather than a tolerance
            const auto day = endDay - (count - 1 - s) * dayPerSample;

            std::array<Vec3d, ORBIT_BODY_COUNT> sample;
            SampleOrbits(day, sample);

            const auto base = static_cast<usize>(s) * ORBIT_BODY_COUNT;
            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                scratch[base + b] = static_cast<Vec3>(sample[b]);
            }
        }

        // one memcpy into this frame's slice; the GPU only sees it once
        // Record turns it into a copy
        pendingStaging = device.UploadTransient(
            std::span<const Vec3>(
                scratch.data(),
                static_cast<usize>(count) * ORBIT_BODY_COUNT
            ),
            static_cast<u32>(sizeof(Vec3))
        );
    }
}
