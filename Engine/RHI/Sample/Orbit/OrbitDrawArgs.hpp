#pragma once

#include <cstddef>
#include <span>

#include "Kepler.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Mirrors BodyDraw in OrbitCommon.slang. Never written after creation, so
    // the per-frame segment counts live in a buffer of their own.
    struct OrbitBodyDraw {
        f32 colorR = 0.0f, colorG = 0.0f, colorB = 0.0f;
        f32 trailPeriodDays = 0.0f;
    };
    static_assert(sizeof(OrbitBodyDraw) == 16);
    static_assert(offsetof(OrbitBodyDraw, trailPeriodDays) == 12);

    // Mirrors OrbitArgsPush in OrbitArgs.slang.
    struct OrbitArgsPush {
        u64 bodies;
        u64 args;
        u64 segCounts;
        u32 bodyCount;
        u32 filled;
        u32 enabledMask;
        f32 orbitTurns;
        f32 dayPerSample;
        u32 _pad0 = 0;
    };
    static_assert(sizeof(OrbitArgsPush) == 48);
    static_assert(offsetof(OrbitArgsPush, bodyCount) == 24);
    static_assert(offsetof(OrbitArgsPush, orbitTurns) == 36);

    class OrbitTrailArgs {
    private:
        RHIComputePipelineStateRAII pso;
        RHIBufferRAII bodies;
        RHIBufferRAII segCounts;
        RHIBufferRAII args;
        u32 count;

        RHIResourceUsage segCountsResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsNextUse;

    public:
        // Release halves; the consumer must acquire with these same values.
        struct Edges {
            RHIBufferBarrier segCounts;
            RHIBufferBarrier args;
        };

        // argsNextUse is fixed: the release half names its consumer
        OrbitTrailArgs(
            RHIDevice&,
            std::span<const OrbitBodyDraw> table,
            RHIResourceUsage argsNextUse = RHIResourceUsage::IndirectArgs
        );
        ~OrbitTrailArgs();
        CROWY_DECLARE_PINNED(OrbitTrailArgs)

        // Opens and closes its own compute pass; call outside any other.
        Edges Record(
            RHICommandList&,
            u32 filled,
            f32 orbitTurns,
            f32 dayPerSample,
            u32 enabledMask
        );

        RHIBuffer& Bodies() const noexcept { return *bodies; }
        RHIBuffer& SegCounts() const noexcept { return *segCounts; }
        RHIBuffer& Args() const noexcept { return *args; }
        u32 DrawCount() const noexcept { return count; }
    };
}
