#pragma once

#include <cstddef>
#include <span>
#include "Kepler.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Mirrors BodyDraw in Engine/Shader/OrbitCommon.slang.
    //
    // Colour and period only, set once at construction and never written
    // again. Keeping the mutable segment counts out of here is what lets this
    // buffer be created with initial contents: a creation upload counts as an
    // access, so a buffer that carries one can never be acquired from
    // Undefined again, and the debug layer says so.
    struct OrbitBodyDraw{
        f32 colorR = 0.0f, colorG = 0.0f, colorB = 0.0f;
        f32 trailPeriodDays = 0.0f;
    };
    static_assert(sizeof(OrbitBodyDraw) == 16);
    static_assert(offsetof(OrbitBodyDraw, trailPeriodDays) == 12);

    // Mirrors OrbitArgsPush in Engine/Shader/OrbitArgs.slang.
    struct OrbitArgsPush{
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

    // The nine trail draws, as arguments the GPU writes for itself.
    //
    // Owns the per-body table and the RHIDrawArgs the ExecuteIndirect reads.
    // `DrawBatch::countBuffer` is reserved for GPU-driven compaction, so the
    // draw count stays fixed at one per body and a switched-off body is an
    // instanceCount of zero rather than a draw that never happens. At nine
    // bodies there is nothing to compact anyway.
    class OrbitTrailArgs{
    private:
        RHIComputePipelineStateRAII pso;
        RHIBufferRAII bodies;
        RHIBufferRAII segCounts;
        RHIBufferRAII args;
        u32 count;

        // what the previous submission left each buffer in
        RHIResourceUsage segCountsResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsNextUse;

    public:
        // The release halves this pass emits. Whatever consumes them has to
        // acquire with these same values - that is what pairs the two ends.
        struct Edges{
            RHIBufferBarrier segCounts;
            RHIBufferBarrier args;
        };

        // `table` supplies the colour and period of each body.
        // `argsNextUse` is what reads the arguments after every Record - the
        // ExecuteIndirect in the sample, a copy in the headless check. Fixed
        // for the lifetime, like OrbitTrail's: the release half has to name
        // the consumer, and a consumer that changed its mind mid-run would
        // have nothing to pair with.
        OrbitTrailArgs(
            RHIDevice&,
            std::span<const OrbitBodyDraw> table,
            RHIResourceUsage argsNextUse = RHIResourceUsage::IndirectArgs
        );
        ~OrbitTrailArgs();
        CROWY_DECLARE_PINNED(OrbitTrailArgs)

        // Opens and closes its own compute pass; call outside any other.
        // `enabledMask` carries one bit per body, so a toggle flipped this
        // frame lands in this frame's draw rather than the next one.
        Edges Record(
            RHICommandList&,
            u32 filled,
            f32 orbitTurns,
            f32 dayPerSample,
            u32 enabledMask
        );

        RHIBuffer& Bodies() const noexcept{ return *bodies; }
        RHIBuffer& SegCounts() const noexcept{ return *segCounts; }
        RHIBuffer& Args() const noexcept{ return *args; }
        u32 DrawCount() const noexcept{ return count; }
    };
}
