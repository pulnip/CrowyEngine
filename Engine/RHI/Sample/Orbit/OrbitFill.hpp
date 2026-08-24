#pragma once

#include <cstddef>
#include <span>
#include <vector>
#include "Kepler.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Mirrors OrbitFillPush in Engine/Shader/OrbitSample.slang.
    struct OrbitFillPush{
        u64 elements;
        u64 phases;
        u64 output;
        u32 bodyCount;
        u32 sampleCount;
        u32 firstSlot;
        u32 capacity;
    };
    static_assert(sizeof(OrbitFillPush) == 40);
    static_assert(offsetof(OrbitFillPush, output) == 16);
    static_assert(offsetof(OrbitFillPush, bodyCount) == 24);

    // Solves a table of Keplerian orbits on the GPU.
    //
    // Owns the element table, which never changes, and the phase table, which
    // is rewritten before every dispatch because it carries the epoch. The
    // caller owns the destination buffer and the barriers around it - this only
    // knows how to fill one.
    //
    // The split matters: the whole reason the solve can move to the GPU is that
    // the phase arrives as fixed-point turns rather than a sim day, and turning
    // a day into turns is a multiply and a floor. What stays on the CPU is
    // cheap; what leaves is Newton and six trigonometric functions per sample.
    class OrbitKeplerFill{
    private:
        RHIComputePipelineStateRAII ringPSO, pointPSO;
        RHIBufferRAII elementBuffer;
        RHIBufferRAII phaseBuffer;

        std::vector<OrbitalElements> elements;
        std::vector<OrbitPhaseGPU> phaseScratch;

    public:
        OrbitKeplerFill(RHIDevice&, std::span<const OrbitalElements>);
        ~OrbitKeplerFill();
        CROWY_DECLARE_PINNED(OrbitKeplerFill)

        u32 Count() const noexcept{
            return static_cast<u32>(elements.size());
        }

        // Rewrites the phase table so sample 0 lands on `day` and sample i on
        // `day + i * dayPerSample`. CPU-side only; the upload rides the
        // dispatch, so this can be called whenever the epoch is known.
        void SetEpoch(f64 day, f64 dayPerSample);

        // Both record inside a compute pass the caller opened, and both leave
        // `output` written through a UAV.
        //
        // A run of `sampleCount` consecutive ring slots from `firstSlot`,
        // wrapping at `capacity`, sample-major with Count() bodies per slot.
        void RecordRing(
            RHICommandList&,
            RHIBuffer& output,
            u32 firstSlot,
            u32 sampleCount,
            u32 capacity
        );
        // One position per element, for tables with no history.
        void RecordPoints(RHICommandList&, RHIBuffer& output);

    private:
        void uploadPhases();
    };
}
