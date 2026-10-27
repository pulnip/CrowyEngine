#include "OrbitTrail.hpp"

#include <algorithm>
#include <array>
#include <span>

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
        : capacity(capacity),
          dayPerSample(dayPerSample),
          device(device),
          nextUse(nextUse) {
        CROWY_ASSERT(
            capacity > 1,
            "a trail needs two samples to have a segment"
        );
        CROWY_ASSERT(dayPerSample > 0.0);
        CROWY_ASSERT(capacity <= 0xFFFFFFFFu / ORBIT_SAMPLE_BYTES);
        CROWY_ASSERT(
            capacity <= ORBIT_PHASE_MAX_STEPS,
            "a prefill walks the fixed-point phase over the whole ring, and "
            "the block split only reaches ORBIT_PHASE_MAX_STEPS"
        );

        const auto ringBytes = capacity * ORBIT_SAMPLE_BYTES;

        trail = device.CreateBuffer(
            RHIBufferCreateDesc{.size = ringBytes, .shaderWrite = true},
            "OrbitTrailRing"
        );

        gpuFill = std::make_unique<OrbitKeplerFill>(device, ORBIT_ELEMENTS);

        scratch.reserve(static_cast<usize>(capacity) * ORBIT_BODY_COUNT);
    }

    OrbitTrail::~OrbitTrail() = default;

    void OrbitTrail::Prefill(f64 endDay) {
        head = 0;
        filled = capacity;
        newestDay = endDay;
        accumDays = 0.0;
        pendingSamples = 0;

        stage(0, capacity, endDay);
    }

    u32 OrbitTrail::Advance(f64 deltaSeconds, f64 daysPerSecond) {
        CROWY_ASSERT(daysPerSecond >= 0.0, "sim time does not run backwards");

        stats.tickCount = 0;
        stats.copyCount = 0;
        accumDays += deltaSeconds * daysPerSecond;

        if(pendingSamples != 0)
            return 0;

        // the copy assumes the write wraps at most once
        const auto maxTicks = std::min(ORBIT_MAX_TICKS_PER_FRAME, capacity);

        u32 ticks = 0;
        while(accumDays >= dayPerSample && ticks < maxTicks) {
            accumDays -= dayPerSample;
            ++ticks;
        }

        if(accumDays >= dayPerSample) {
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

    void OrbitTrail::SetDayPerSample(f64 value) {
        CROWY_ASSERT(value > 0.0);

        if(value == dayPerSample)
            return;

        dayPerSample = value;
        Prefill(newestDay);
    }

    std::optional<RHIBufferBarrier> OrbitTrail::Record(
        RHICommandList& cmdList
    ) {
        stats.copyCount = 0;
        stats.dispatchCount = 0;

        if(pendingSamples == 0)
            return std::nullopt;

        const auto firstRun =
            std::min(pendingSamples, capacity - pendingFirstSlot);
        const auto secondRun = pendingSamples - firstRun;

        const auto writeAs = pendingGpu ? RHIResourceUsage::StorageCompute
                                        : RHIResourceUsage::CopyDst;

        // a partial write: the acquire must not discard
        const auto acquire =
            resting == RHIResourceUsage::Undefined
                ? makeBarrier(*trail, RHIResourceUsage::Undefined, writeAs)
                : makeCrossSubmissionBarrier(*trail, resting, writeAs);
        const auto release = makeBarrier(*trail, writeAs, nextUse);

        const std::array acquires{acquire};
        const std::array releases{release};

        if(pendingGpu) {
            cmdList.BeginComputePass({}, acquires);

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
        if(secondRun > 0) {
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

    void OrbitTrail::stage(u32 firstSlot, u32 count, f64 endDay) {
        CROWY_ASSERT(pendingSamples == 0);
        CROWY_ASSERT(0 < count && count <= capacity);
        CROWY_ASSERT(firstSlot < capacity);

        pendingFirstSlot = firstSlot;
        pendingSamples = count;
        pendingGpu = fillMode == OrbitFillMode::Gpu;

        if(pendingGpu) {
            gpuFill->SetEpoch(
                endDay - (count - 1) * dayPerSample,
                dayPerSample
            );

            return;
        }

        scratch.resize(static_cast<usize>(count) * ORBIT_BODY_COUNT);

        for(u32 s = 0; s < count; ++s) {
            // the same expression as DayForAge, so a readback compares
            // bit-exactly
            const auto day = endDay - (count - 1 - s) * dayPerSample;

            std::array<Vec3d, ORBIT_BODY_COUNT> sample;
            SampleOrbits(day, sample);

            const auto base = static_cast<usize>(s) * ORBIT_BODY_COUNT;
            for(u32 b = 0; b < ORBIT_BODY_COUNT; ++b) {
                scratch[base + b] = static_cast<Vec3>(sample[b]);
            }
        }

        pendingStaging = device.UploadTransient(
            std::span<const Vec3>(
                scratch.data(),
                static_cast<usize>(count) * ORBIT_BODY_COUNT
            ),
            static_cast<u32>(sizeof(Vec3))
        );
    }
}
