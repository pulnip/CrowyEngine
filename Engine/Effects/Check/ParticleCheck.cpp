#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <print>
#include <span>
#include <vector>

#include "EffectRandom.hpp"
#include "EffectSystem.hpp"
#include "FieldBuffer.hpp"
#include "ParticleEffects.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

// Runs ParticleCheck.slang through EffectSystem for hundreds of steps and
// replays every slot's births and ages on the CPU, which must agree exactly.

namespace
{
    using namespace Crowy;
    using Particles = std::vector<EffectParticle>;

    constexpr u32 Count = 2048;
    constexpr u32 Seed = 7;
    constexpr u32 Prewarm = 3;
    constexpr u32 Frames = 300;

    // ParticleCheck.slang's spawn
    u32 lifeOf(u32 slot, u32 generation) {
        return 20 + effectHash(Seed, slot, generation, 1) % 40;
    }

    // ParticleKernel.slang's step, `steps` times from step 0
    EffectParticle replay(u32 slot, u32 steps) {
        EffectParticle p{.lifeSteps = lifeOf(slot, 0)};
        p.ageSteps =
            effectHash(Seed, slot, 0, ParticleStaggerDraw) % p.lifeSteps;
        for(u32 step = 1; step < steps; ++step) {
            ++p.ageSteps;
            if(p.ageSteps >= p.lifeSteps) {
                const auto generation = p.generation + 1;
                p = EffectParticle{
                    .lifeSteps = lifeOf(slot, generation),
                    .generation = generation
                };
                continue;
            }
            p.position.z += 1.0f;
        }
        p.position.x = static_cast<f32>(slot);
        p.position.y = static_cast<f32>(p.generation);

        return p;
    }

    bool same(const EffectParticle& gpu, const EffectParticle& cpu) {
        return gpu.ageSteps == cpu.ageSteps &&
               gpu.lifeSteps == cpu.lifeSteps &&
               gpu.generation == cpu.generation &&
               gpu.position.x == cpu.position.x &&
               gpu.position.y == cpu.position.y &&
               gpu.position.z == cpu.position.z;
    }
}

int main() {
    try {
        auto device = CreateDevice();
        auto cmdList = device->CreateCommandList();
        EffectSystem effects(*device, RHIResourceUsage::CopySrc);
        effects.Add(
            ParticleEffectDesc{
                .name = "replayed",
                .shader = "Engine/Effects/Check/ParticleCheck.slang",
                .count = Count,
                .seed = Seed,
                .prewarmSteps = Prewarm
            }
        );
        auto& particles = effects.Particles("replayed");
        auto readback = device->CreateBuffer(
            RHIBufferCreateDesc{
                .size = particles.Bytes(),
                .memory = RHIMemoryType::CPURead
            },
            "ParticleCheck.readback"
        );

        for(u32 frame = 1; frame <= Frames; ++frame) {
            cmdList->Begin();
            const auto releases = effects.Simulate(*cmdList, EffectView{});
            if(frame == Frames) {
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
            device->Submit(lists, frame);
            device->WaitFrame(frame);
        }

        Particles gpu(Count);
        readback->Download(gpu.data(), particles.Bytes());
        const auto steps = effects.Steps("replayed");
        u32 wrong = 0;
        u64 births = 0;
        for(u32 slot = 0; slot < Count; ++slot) {
            const auto cpu = replay(slot, steps);
            births += cpu.generation;
            if(!same(gpu[slot], cpu)) {
                if(wrong == 0) {
                    std::println(
                        "  slot {}: gpu age {} life {} generation {} z {}, "
                        "cpu age {} life {} generation {} z {}",
                        slot,
                        gpu[slot].ageSteps,
                        gpu[slot].lifeSteps,
                        gpu[slot].generation,
                        gpu[slot].position.z,
                        cpu.ageSteps,
                        cpu.lifeSteps,
                        cpu.generation,
                        cpu.position.z
                    );
                }
                ++wrong;
            }
        }

        std::println(
            "ParticleCheck: {} steps, {} rebirths, {} of {} slots differ",
            steps,
            births,
            wrong,
            Count
        );

        return wrong == 0 && steps == Prewarm + Frames ? 0 : 1;
    } catch(const std::exception& e) {
        std::println(stderr, "ParticleCheck: {}", e.what());

        return 1;
    }
}
