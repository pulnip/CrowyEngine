#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <exception>
#include <print>
#include <span>
#include <vector>

#include "EffectRandom.hpp"
#include "EffectSystem.hpp"
#include "FieldBuffer.hpp"
#include "Island/IslandEffects.hpp"
#include "Island/IslandShapes.hpp"
#include "Island/Sea.hpp"
#include "Island/Weather.hpp"
#include "ParticleEffects.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "WorldClock.hpp"

// Runs particle effects and holds what they left against the CPU: a
// lifecycle replayed slot by slot, the rain's falls, the wind's probes.

namespace
{
    using namespace Crowy;

    struct Snapshot;

    using Particles = std::vector<EffectParticle>;
    using Snapshots = std::vector<Snapshot>;

    constexpr u32 Frames = 300;
    // ParticleCheck.slang's life, which the CPU replays
    constexpr u32 ReplaySeed = 7;
    // how far a CPU fall may stray from the GPU's, across and up
    constexpr f32 Across = 2.0e-3f;
    constexpr f32 Up = 1.0e-3f;

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

    // Rain.slang's surface at a world step: the sea's grid waves, the sand's
    // crown, or the tipi's pyramid
    f32 surfaceHeight(Vec3 at, u32 world) {
        const Vec2 xz{at.x, at.z};
        const auto sea = seaHeight(xz, world, SEA_VERTEX_WAVES, true);

        return std::max(std::max(sea, islandCrown(xz)), tipiRoof(xz));
    }

    f32 clearance(Vec3 at, u32 world) {
        return at.y - surfaceHeight(at, world);
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

    // Rain.slang's step: across with the wind at `world`, down at its speed
    Vec3 stepped(const ParticleEffectDesc& rain, Vec3 at, u32 world) {
        const auto wind = windAt(Vec2{at.x, at.z}, world);

        return at + Vec3{wind.x, -rain.params[0].z, wind.y} * EffectStep;
    }

    bool near(Vec3 lhs, Vec3 rhs) {
        return std::abs(lhs.x - rhs.x) <= Across &&
               std::abs(lhs.y - rhs.y) <= Up &&
               std::abs(lhs.z - rhs.z) <= Across;
    }

    // what an effect left after a frame, and the steps it had run by then
    struct Snapshot {
        u32 frame = 0;
        u32 steps = 0;
        Particles particles;
    };

    // frames of one effect at world steps worldBase + frame, its particles
    // copied out after each frame `copies` names
    Snapshots run(
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
            "ParticleCheck.readback"
        );

        // the device counts frames across the program
        static u64 submitted = 0;
        Snapshots snapshots;
        for(u32 frame = 1; frame <= copies.back(); ++frame) {
            const bool copied =
                std::ranges::find(copies, frame) != copies.end();
            cmdList->Begin();
            const auto releases =
                effects.Simulate(*cmdList, EffectView{}, worldBase + frame);
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
                Snapshot{
                    .frame = frame,
                    .steps = effects.Steps(name),
                    .particles = Particles(count)
                }
            );
            readback->Download(snapshot.particles.data(), particles.Bytes());
        }

        return snapshots;
    }

    // every slot's age, life, generation and history equal the CPU's
    bool checkReplay(RHIDevice& device) {
        constexpr u32 Count = 2048;
        constexpr u32 ReplayPrewarm = 3;
        constexpr std::array Copies{Frames};

        const auto snapshot = run(
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
        ).front();
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

    // the Island's rain at one snapshot: each drop's fall since its landing
    // replayed from the box's top, and the landing from the fall before
    bool checkRain(
        const ParticleEffectDesc& rain,
        const Snapshot& snapshot,
        u32 worldBase
    ) {
        const auto last = snapshot.steps - 1;
        // the world step an effect step ran at
        const auto worldOf = [&](u32 step) {
            return step + worldBase + 1 - rain.prewarmSteps;
        };

        u32 unlanded = 0;
        u32 offFall = 0;
        u32 offLanding = 0;
        u32 below = 0;
        // touches the CPU sees a step from the GPU's, within tolerance
        u32 nearThreshold = 0;
        // a step that met the surface where the GPU's did not; true if only
        // by the tolerance, which it counts
        const auto grazes = [&](f32 gap) {
            const bool grazing = gap > -Up;
            nearThreshold += grazing ? 1 : 0;
            return grazing;
        };
        for(u32 slot = 0; slot < rain.count; ++slot) {
            const auto& drop = snapshot.particles[slot];
            below += clearance(drop.position, worldOf(last)) > -Up ? 0 : 1;
            if(drop.generation == 0) {
                ++unlanded;
                continue;
            }

            // since its landing, from the box's top
            const auto landing = last - drop.ageSteps;
            auto at = dropStart(rain, slot, drop.generation);
            bool fell = true;
            for(u32 step = landing + 1; step <= last && fell; ++step) {
                at = stepped(rain, at, worldOf(step));
                const auto gap = clearance(at, worldOf(step));
                if(gap <= 0.0f)
                    fell = grazes(gap);
            }
            offFall += fell && near(at, drop.position) ? 0 : 1;

            // the fall before, from its birth to the step it met the surface
            const auto birth = static_cast<u32>(drop.custom.w);
            bool landed = drop.custom.w == static_cast<f32>(birth) &&
                          birth < landing &&
                          (drop.generation > 1 || birth == 0);
            at = dropStart(rain, slot, drop.generation - 1);
            for(u32 step = birth + 1; step < landing && landed; ++step) {
                at = stepped(rain, at, worldOf(step));
                const auto gap = clearance(at, worldOf(step));
                if(gap <= 0.0f)
                    landed = grazes(gap);
            }
            if(landed) {
                const auto touch = stepped(rain, at, worldOf(landing));
                const auto gap = clearance(touch, worldOf(landing));
                if(gap > 0.0f)
                    landed = grazes(-gap);
                const Vec3 onSurface{
                    touch.x,
                    surfaceHeight(touch, worldOf(landing)),
                    touch.z
                };
                const Vec3 recorded{
                    drop.custom.x,
                    drop.custom.y,
                    drop.custom.z
                };
                landed = landed && near(onSurface, recorded);
            }
            offLanding += landed ? 0 : 1;
        }

        const bool passed = unlanded == 0 && offFall == 0 && offLanding == 0 &&
                            below == 0 && nearThreshold <= rain.count / 100;
        std::println(
            "  rain at frame {} from world step {}: {} ({} of {} landed, {} "
            "landings off, {} falls off, {} below the surface, {} near the "
            "threshold)",
            snapshot.frame,
            worldBase,
            passed ? "ok" : "FAIL",
            rain.count - unlanded,
            rain.count,
            offLanding,
            offFall,
            below,
            nearThreshold
        );

        return passed;
    }

    // WeatherProbe.slang's probes: every slot's wind and sea at its own place
    // and world step held against the CPU's
    bool checkWeather(RHIDevice& device) {
        constexpr u32 Places = 64 * 64;
        constexpr u32 ProbedSteps = 10;
        constexpr f32 Tolerance = 5.0e-4f;
        constexpr std::array Copies{1u};

        const ParticleEffectDesc probes{
            .name = "probes",
            .shader = "Engine/Effects/Sample/Island/WeatherProbe.slang",
            .count = Places * ProbedSteps,
            .seed = 1,
            .emitter = Vec4{0.0f, 0.0f, 0.0f, 20.0f}
        };
        const auto snapshot = run(device, probes, 0, Copies).front();

        f32 windError = 0.0f;
        f32 seaError = 0.0f;
        for(const auto& probe: snapshot.particles) {
            const auto step = std::bit_cast<u32>(probe.custom.x);
            const Vec2 xz{probe.position.x, probe.position.z};
            const auto wind = windAt(xz, step);
            const auto sea = seaHeight(xz, step, SEA_VERTEX_WAVES, true);
            windError = std::max(
                {windError,
                 std::abs(wind.x - probe.velocity.x),
                 std::abs(wind.y - probe.velocity.z)}
            );
            seaError = std::max(seaError, std::abs(sea - probe.custom.y));
        }

        const bool passed = windError <= Tolerance && seaError <= Tolerance;
        std::println(
            "  weather: {} (the wind within {:.1e} m/s and the sea within "
            "{:.1e} m of the CPU's at {} places and {} world steps)",
            passed ? "ok" : "FAIL",
            windError,
            seaError,
            Places,
            ProbedSteps
        );

        return passed;
    }
}

int main() {
    try {
        auto device = CreateDevice();
        std::println("ParticleCheck");

        const auto rain = rainDesc();
        constexpr std::array RainCopies{1u, 60u, Frames};
        // a later start, so the falls cross the edge of the world's loop
        constexpr u32 LateBase = LoopSteps - 200;
        constexpr std::array LateCopies{Frames};
        const auto early = run(*device, rain, 0, RainCopies);
        const auto late = run(*device, rain, LateBase, LateCopies);

        std::vector<bool> results{checkReplay(*device)};
        for(const auto& snapshot: early)
            results.push_back(checkRain(rain, snapshot, 0));
        results.push_back(checkRain(rain, late.front(), LateBase));
        results.push_back(checkWeather(*device));
        const auto passed = std::ranges::count(results, true);
        std::println("ParticleCheck: {} of {} passed", passed, results.size());

        return static_cast<usize>(passed) == results.size() ? 0 : 1;
    } catch(const std::exception& e) {
        std::println(stderr, "ParticleCheck: {}", e.what());

        return 1;
    }
}
