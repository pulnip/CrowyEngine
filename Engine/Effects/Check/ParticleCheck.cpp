#include <algorithm>
#include <array>
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

// Runs particle effects through EffectSystem for hundreds of steps and holds
// what they left against the CPU: a lifecycle replayed slot by slot, and
// rain that lands exactly on the island.

namespace
{
    using namespace Crowy;
    using Particles = std::vector<EffectParticle>;

    constexpr u32 Frames = 300;

    // ParticleCheck.slang's life, which the CPU replays
    constexpr u32 ReplaySeed = 7;
    constexpr u32 ReplayPrewarm = 3;

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

    // Rain.slang's surface: the sea at 0, or the ellipsoid's crown above it
    f32 surfaceHeight(Vec4 island, f32 x, f32 z) {
        const auto qx = x / island.x;
        const auto qz = z / island.z;
        const auto inside = 1.0f - (qx * qx + qz * qz);
        if(inside <= 0.0f)
            return 0.0f;

        return std::max(0.0f, island.w + island.y * std::sqrt(inside));
    }

    // `frames` frames of one effect, its particles copied out after the last
    Particles run(RHIDevice& device, ParticleEffectDesc desc, u32& steps) {
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
            "ParticleCheck.readback"
        );

        // the device counts frames across the program
        static u64 submitted = 0;
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
            device.Submit(lists, ++submitted);
            device.WaitFrame(submitted);
        }

        Particles out(count);
        readback->Download(out.data(), particles.Bytes());
        steps = effects.Steps(name);

        return out;
    }

    // every slot's age, life, generation and history equal the CPU's
    bool checkReplay(RHIDevice& device) {
        constexpr u32 Count = 2048;
        u32 steps = 0;
        const auto gpu = run(
            device,
            ParticleEffectDesc{
                .name = "replayed",
                .shader = "Engine/Effects/Check/ParticleCheck.slang",
                .count = Count,
                .seed = ReplaySeed,
                .prewarmSteps = ReplayPrewarm
            },
            steps
        );

        u32 wrong = 0;
        u64 births = 0;
        for(u32 slot = 0; slot < Count; ++slot) {
            const auto cpu = replay(slot, steps);
            births += cpu.generation;
            if(same(gpu[slot], cpu))
                continue;
            if(wrong++ == 0) {
                std::println(
                    "  slot {}: gpu age {} life {} generation {}, cpu age {} "
                    "life {} generation {}",
                    slot,
                    gpu[slot].ageSteps,
                    gpu[slot].lifeSteps,
                    gpu[slot].generation,
                    cpu.ageSteps,
                    cpu.lifeSteps,
                    cpu.generation
                );
            }
        }

        const bool passed = wrong == 0 && steps == ReplayPrewarm + Frames;
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

    // the Island sample's rain: every landing on the surface, every drop
    // above it, and most drops landed at least once
    bool checkRain(RHIDevice& device) {
        constexpr u32 Count = 3000;
        constexpr Vec4 IslandShape{9.0f, 1.6f, 7.0f, -1.2f};
        constexpr f32 NoLanding = -1.0e6f;
        u32 steps = 0;
        const auto gpu = run(
            device,
            ParticleEffectDesc{
                .name = "rain",
                .shader = "Engine/Effects/Sample/Island/Rain.slang",
                .count = Count,
                .seed = 23,
                .prewarmSteps = 120,
                .emitter = Vec4{0.0f, 6.5f, 2.0f, 6.0f},
                .params =
                    {Vec4{8.0f, 8.0f, 9.0f, 1.2f},
                     Vec4{0.4f, 0.03f, 0.35f, 0.28f},
                     IslandShape}
            },
            steps
        );

        u32 landed = 0;
        u32 offSurface = 0;
        u32 belowSurface = 0;
        for(const auto& drop: gpu) {
            if(drop.custom.w > NoLanding * 0.5f) {
                ++landed;
                const auto ground =
                    surfaceHeight(IslandShape, drop.custom.x, drop.custom.z);
                const bool onSurface =
                    std::abs(drop.custom.y - ground) <= 1e-3f;
                const bool inTime =
                    drop.custom.w >= 0.0f &&
                    drop.custom.w < static_cast<f32>(steps);
                offSurface += onSurface && inTime ? 0 : 1;
            }
            const auto under = surfaceHeight(
                IslandShape,
                drop.position.x,
                drop.position.z
            );
            belowSurface += drop.position.y > under - 1e-3f ? 0 : 1;
        }

        const bool passed = offSurface == 0 && belowSurface == 0 &&
                            landed > Count / 2;
        std::println(
            "  rain: {} ({} of {} landed, {} landings off the surface, {} "
            "drops below it)",
            passed ? "ok" : "FAIL",
            landed,
            Count,
            offSurface,
            belowSurface
        );

        return passed;
    }
}

int main() {
    try {
        auto device = CreateDevice();
        std::println("ParticleCheck");
        const std::array results{checkReplay(*device), checkRain(*device)};
        const auto passed = std::ranges::count(results, true);
        std::println("ParticleCheck: {} of {} passed", passed, results.size());

        return static_cast<usize>(passed) == results.size() ? 0 : 1;
    } catch(const std::exception& e) {
        std::println(stderr, "ParticleCheck: {}", e.what());

        return 1;
    }
}
