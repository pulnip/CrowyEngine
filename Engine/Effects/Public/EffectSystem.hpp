#pragma once

#include <memory>
#include <span>
#include <vector>

#include "ComputeKernel.hpp"
#include "FieldBuffer.hpp"
#include "FieldPass.hpp"
#include "FramePipeline.hpp"
#include "ParticleEffects.hpp"
#include "PipelineCache.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // Particle effects stepped in steps of 1/60 s and drawn in a hook pass:
    // step N of an effect depends only on N, its seed and its desc.
    class EffectSystem {
    private:
        // one effect's description, its particles and its kernel
        class Effect {
        public:
            ParticleEffectDesc desc;
            FieldBuffer particles;
            // replaced whole by a reload
            std::unique_ptr<ComputeKernel> step;
            // its next step, counted from 0
            u32 next = 0;

        public:
            Effect(
                RHIDevice& device,
                ParticleEffectDesc desc,
                RHIResourceUsage reader
            );
        };

        using Effects = std::vector<std::unique_ptr<Effect>>;
        using Fields = std::vector<FieldBuffer*>;
        using Barriers = std::vector<RHIBufferBarrier>;
        using Kernels = std::vector<std::unique_ptr<ComputeKernel>>;

    private:
        RHIDevice& device;
        FieldKernels kernels;
        // the particles' release usage: what draws them, or a check's copy
        RHIResourceUsage reader = RHIResourceUsage::SampledVertex;
        Effects effects;
        Fields fields;
        // the frame's camera, which the draws' push repeats
        EffectView view;
        // the frame's world step, which the draws show even while paused
        u32 worldStep = 0;
        // the frame's releases, which outlive the pass that made them
        Barriers releases;
        bool paused = false;

    public:
        ~EffectSystem();
        CROWY_DECLARE_PINNED(EffectSystem)

        explicit EffectSystem(
            RHIDevice& device,
            RHIResourceUsage reader = RHIResourceUsage::SampledVertex
        );

        // throws std::invalid_argument for no name, no particles or more than
        // a dispatch reaches, a name taken, or a draw without an entry
        void Add(ParticleEffectDesc desc);

        // `steps` steps of each effect up to worldStep, a new one its prewarm
        // and one; paused, only the new run; returns the hook pass's acquires
        std::span<const RHIBufferBarrier> Simulate(
            RHICommandList& cmdList,
            const EffectView& view,
            u32 worldStep,
            u32 steps
        );
        // each effect's draws in the order added; returns how many
        u32 Draw(
            RHICommandList& cmdList,
            const HookPassContext& context,
            PipelineCache& pipelines
        );

        // every cs_step compiled again and swapped in only when all built,
        // the old retired after the frames in flight; returns how many
        usize ReloadKernels();

        void SetPaused(bool paused) noexcept { this->paused = paused; }
        bool Paused() const noexcept { return paused; }
        // the effect of that name's particles, for a check to copy out
        FieldBuffer& Particles(StrView name);
        // the steps the effect of that name has run
        u32 Steps(StrView name) const;

    private:
        // an effect's push with the frame's camera; the step is the caller's
        EffectPush pushOf(Effect& effect) const;
        Effect& find(StrView name) const;
    };
}
