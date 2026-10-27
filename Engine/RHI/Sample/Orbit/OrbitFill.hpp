#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "Kepler.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Mirrors OrbitFillPush in OrbitSample.slang.
    struct OrbitFillPush {
        u64 elements;
        u64 phases;
        u64 output;
        u32 bodyCount;
        u32 sampleCount;
        u32 firstSlot;
        u32 capacity;
        u32 phaseBase;
        u32 _pad0;
    };
    static_assert(sizeof(OrbitFillPush) == 48);
    static_assert(offsetof(OrbitFillPush, output) == 16);
    static_assert(offsetof(OrbitFillPush, bodyCount) == 24);
    static_assert(offsetof(OrbitFillPush, phaseBase) == 40);

    class OrbitKeplerFill {
    private:
        RHIComputePipelineStateRAII ringPSO, pointPSO;
        RHIBufferRAII elementBuffer;
        RHIDevice& device;
        RHIBufferSlice phaseSlice;

        std::vector<OrbitalElements> elements;
        std::vector<OrbitPhaseGPU> phaseScratch;

    public:
        OrbitKeplerFill(RHIDevice&, std::span<const OrbitalElements>);
        ~OrbitKeplerFill();
        CROWY_DECLARE_PINNED(OrbitKeplerFill)

        u32 Count() const noexcept { return static_cast<u32>(elements.size()); }

        // CPU-side only; the upload rides the next Record.
        void SetEpoch(f64 day, f64 dayPerSample);

        // Both record inside the caller's compute pass; `output` is a UAV.
        void RecordRing(
            RHICommandList&,
            RHIBuffer& output,
            u32 firstSlot,
            u32 sampleCount,
            u32 capacity
        );
        void RecordPoints(RHICommandList&, RHIBuffer& output);

    private:
        void uploadPhases();
    };
}
