#include <array>
#include <exception>
#include <print>

#include "EffectRandom.hpp"
#include "ParticleEffects.hpp"
#include "ParticleRun.hpp"
#include "RHIDevice.hpp"

// Runs a particle effect and replays its lifecycle on the CPU, slot by slot.

namespace
{
    using namespace Crowy;

    // ParticleCheck.slang's life, which the CPU replays
    constexpr u32 ReplaySeed = 7;

    u32 lifeOf(u32 slot, u32 generation) {
        return 20 + effectHash(ReplaySeed, slot, generation, 1) % 40;
    }

    // ParticleKernel.slang's step, `steps` times from step 0
    EffectParticle replay(u32 slot, u32 steps) {
        EffectParticle p{.lifeSteps = lifeOf(slot, 0)};
        p.ageSteps =
            effectHash(ReplaySeed, slot, 0, ParticleStaggerDraw) % p.lifeSteps;
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

    // every slot's age, life, generation and history equal the CPU's
    bool checkReplay(RHIDevice& device) {
        constexpr u32 Count = 2048;
        constexpr u32 ReplayPrewarm = 3;
        constexpr std::array Copies{CheckFrames};

        const auto snapshot =
            runParticles(
                device,
                ParticleEffectDesc{
                    .name = "replayed",
                    .shader = "Engine/Effects/Check/ParticleCheck.slang",
                    .count = Count,
                    .seed = ReplaySeed,
                    .prewarmSteps = ReplayPrewarm
                },
                0,
                Copies
            )
                .front();
        const auto& gpu = snapshot.particles;
        const auto steps = snapshot.steps;

        u32 wrong = 0;
        u64 births = 0;
        for(u32 slot = 0; slot < Count; ++slot) {
            const auto cpu = replay(slot, steps);
            births += cpu.generation;
            if(same(gpu[slot], cpu))
                continue;
            if(wrong++ == 0) {
                std::println(
                    "  slot {}: gpu age {} life {} generation {} moves {}, "
                    "cpu age {} life {} generation {} moves {}",
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
        }

        const bool passed = wrong == 0 && steps == ReplayPrewarm + CheckFrames;
        std::println(
            "  replay: {} ({} steps, {} rebirths, {} of {} slots differ)",
            passed ? "ok" : "FAIL",
            steps,
            births,
            wrong,
            Count
        );

        return passed;
    }
}

int main() {
    try {
        auto device = CreateDevice();
        std::println("ParticleCheck");

        const bool passed = checkReplay(*device);
        std::println("ParticleCheck: {} of 1 passed", passed ? 1 : 0);

        return passed ? 0 : 1;
    } catch(const std::exception& e) {
        std::println(stderr, "ParticleCheck: {}", e.what());

        return 1;
    }
}
