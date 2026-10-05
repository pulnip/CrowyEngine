#pragma once

#include <algorithm>
#include <span>
#include <vector>

#include "EffectSystem.hpp"
#include "FieldBuffer.hpp"
#include "ParticleEffects.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

// Runs one particle effect frame by frame and copies its particles out, for
// the checks that hold them against the CPU.

namespace Crowy
{
    struct ParticleSnapshot;

    using Particles = std::vector<EffectParticle>;
    using ParticleSnapshots = std::vector<ParticleSnapshot>;

    inline constexpr u32 CheckFrames = 300;

    // what an effect left after a frame, and the steps it had run by then
    struct ParticleSnapshot {
        u32 frame = 0;
        u32 steps = 0;
        Particles particles;
    };

    // frames of one effect at world steps worldBase + frame, its particles
    // copied out after each frame `copies` names
    inline ParticleSnapshots runParticles(
        RHIDevice& device,
        ParticleEffectDesc desc,
        u32 worldBase,
        std::span<const u32> copies
    ) {
        auto cmdList = device.CreateCommandList();
        EffectSystem effects(device, RHIResourceUsage::CopySrc);
        const auto name = desc.name;
        const auto count = desc.count;
        effects.Add(std::move(desc));
        auto& particles = effects.Particles(name);
        auto readback = device.CreateBuffer(
            RHIBufferCreateDesc{
                .size = particles.Bytes(),
                .memory = RHIMemoryType::CPURead
            },
            "ParticleRun.readback"
        );

        // the device counts frames across the program
        static u64 submitted = 0;
        ParticleSnapshots snapshots;
        for(u32 frame = 1; frame <= copies.back(); ++frame) {
            const bool copied =
                std::ranges::find(copies, frame) != copies.end();
            cmdList->Begin();
            const auto releases =
                effects.Simulate(*cmdList, EffectView{}, worldBase + frame, 1);
            if(copied) {
                cmdList->BeginBlitPass({}, releases);
                cmdList->Copy(
                    particles.Buffer(),
                    *readback,
                    0,
                    0,
                    particles.Bytes()
                );
                cmdList->EndBlitPass();
            }
            cmdList->Close();
            RHICommandList* lists[] = {cmdList.get()};
            device.Submit(lists, ++submitted);
            device.WaitFrame(submitted);
            if(!copied)
                continue;

            auto& snapshot = snapshots.emplace_back(
                ParticleSnapshot{
                    .frame = frame,
                    .steps = effects.Steps(name),
                    .particles = Particles(count)
                }
            );
            readback->Download(snapshot.particles.data(), particles.Bytes());
        }

        return snapshots;
    }
}
