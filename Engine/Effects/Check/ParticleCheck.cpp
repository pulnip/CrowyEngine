#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <print>
#include <vector>

#include "EffectRandom.hpp"
#include "EffectSystem.hpp"
#include "FieldBuffer.hpp"
#include "ParticleEffects.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

// Runs particle effects for hundreds of steps and holds what they left against
// the CPU: a lifecycle replayed slot by slot, and rain's falls and landings.

namespace
{
    using namespace Crowy;
    using Particles = std::vector<EffectParticle>;

    constexpr u32 Frames = 300;

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

    // Rain.slang's surface: the sea at 0, or the ellipsoid's crown above it
    f32 surfaceHeight(Vec4 island, Vec3 at) {
        const auto qx = at.x / island.x;
        const auto qz = at.z / island.z;
        const auto inside = 1.0f - (qx * qx + qz * qz);
        if(inside <= 0.0f)
            return 0.0f;

        return std::max(0.0f, island.w + island.y * std::sqrt(inside));
    }

    // Rain.slang's drop: anywhere in the box when first born, at its top after
    Vec3 dropStart(const ParticleEffectDesc& rain, u32 slot, u32 generation) {
        const auto spread = [&](u32 k) {
            return effectRandom(rain.seed, slot, generation, k) * 2.0f - 1.0f;
        };
        const auto box = rain.emitter;
        const auto extent = rain.params[0];
        const auto height = generation == 0 ? spread(3) * box.w : box.w;

        return Vec3{
            box.x + spread(1) * extent.x,
            box.y + height,
            box.z + spread(2) * extent.y
        };
    }

    bool near(Vec3 lhs, Vec3 rhs, f32 tolerance) {
        return std::abs(lhs.x - rhs.x) <= tolerance &&
               std::abs(lhs.y - rhs.y) <= tolerance &&
               std::abs(lhs.z - rhs.z) <= tolerance;
    }

    // `Frames` frames of one effect, its particles copied out after the last
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
            const auto releases =
                effects.Simulate(*cmdList, EffectView{}, frame);
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
        constexpr u32 ReplayPrewarm = 3;
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

    // the Island's rain: every drop fell from the box's top since it landed,
    // and that landing is where its last fall first met the surface
    bool checkRain(RHIDevice& device) {
        constexpr u32 Count = 3000;
        constexpr f32 NoLanding = -1.0e6f;
        constexpr f32 Tolerance = 1.0e-3f;
        const ParticleEffectDesc rain{
            .name = "rain",
            .shader = "Engine/Effects/Sample/Island/Rain.slang",
            .count = Count,
            .seed = 23,
            .prewarmSteps = 120,
            .emitter = Vec4{0.0f, 6.5f, 2.0f, 6.0f},
            .params =
                {Vec4{8.0f, 8.0f, 9.0f, 1.2f},
                 Vec4{0.4f, 0.03f, 0.35f, 0.28f},
                 Vec4{9.0f, 1.6f, 7.0f, -1.2f}}
        };
        const auto island = rain.params[2];
        const auto stepFall =
            Vec3{rain.params[0].w, -rain.params[0].z, rain.params[1].x} *
            EffectStep;
        const auto stepAcross = std::hypot(stepFall.x, stepFall.z);

        u32 steps = 0;
        const auto gpu = run(device, rain, steps);

        u32 landed = 0;
        u32 offFall = 0;
        u32 offLanding = 0;
        u32 belowSurface = 0;
        for(u32 slot = 0; slot < Count; ++slot) {
            const auto& drop = gpu[slot];
            const auto under = surfaceHeight(island, drop.position);
            belowSurface += drop.position.y > under - Tolerance ? 0 : 1;
            if(drop.generation == 0 || drop.custom.w <= NoLanding * 0.5f)
                continue;

            ++landed;
            const auto landingStep = drop.custom.w;
            const auto top = dropStart(rain, slot, drop.generation);
            const auto finalStep = static_cast<f32>(steps - 1);
            const auto landedAt =
                static_cast<u32>(std::clamp(landingStep, 0.0f, finalStep));
            const auto since = steps - 1 - landedAt;
            const auto fallen = top + stepFall * static_cast<f32>(since);
            const bool onFall = near(drop.position, fallen, Tolerance) &&
                                drop.ageSteps == since;
            offFall += onFall ? 0 : 1;

            const auto landing =
                Vec3{drop.custom.x, drop.custom.y, drop.custom.z};
            const auto from = dropStart(rain, slot, drop.generation - 1);
            const auto across =
                std::hypot(landing.x - from.x, landing.z - from.z);
            const auto moves = std::round(across / stepAcross);
            const auto last = from + stepFall * moves;
            const auto before = from + stepFall * (moves - 1.0f);
            const bool onLastFall =
                moves >= 1.0f && std::abs(last.x - landing.x) <= Tolerance &&
                std::abs(last.z - landing.z) <= Tolerance;
            const bool onSurface =
                std::abs(landing.y - surfaceHeight(island, landing)) <=
                Tolerance;
            const bool firstTouch =
                last.y <= landing.y + Tolerance &&
                before.y > surfaceHeight(island, before) - Tolerance;
            const bool inTime = landingStep >= 0.0f &&
                                landingStep < static_cast<f32>(steps) &&
                                landingStep == std::floor(landingStep);
            offLanding +=
                onLastFall && onSurface && firstTouch && inTime ? 0 : 1;
        }

        const bool passed = landed == Count && offFall == 0 &&
                            offLanding == 0 && belowSurface == 0;
        std::println(
            "  rain: {} ({} of {} landed, {} landings off their fall or the "
            "surface, {} drops off their fall, {} below the surface)",
            passed ? "ok" : "FAIL",
            landed,
            Count,
            offLanding,
            offFall,
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
