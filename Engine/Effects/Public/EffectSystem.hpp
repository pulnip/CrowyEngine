#pragma once

#include <memory>
#include <span>
#include <vector>

#include "ComputeKernel.hpp"
#include "FieldBuffer.hpp"
#include "FieldPass.hpp"
#include "FrameGraph.hpp"
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
        class Effect {
        public:
            ParticleEffectDesc desc;
            FieldBuffer particles;
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
        FieldBuffer& Particles(StrView name);
        u32 Steps(StrView name) const;
        // where the effect of that name sits; a move reaches its next step
        // and draw, while the particles already born stay put
        Vec3 Origin(StrView name) const;
        void SetOrigin(StrView name, Vec3 origin);

    private:
        // an effect's push with the frame's camera; the step is the caller's
        EffectPush pushOf(Effect& effect) const;
        Effect& find(StrView name) const;
    };
}
