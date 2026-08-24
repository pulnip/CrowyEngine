#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <print>
#include <span>
#include <vector>
#include "Kepler.hpp"
#include "OrbitDrawArgs.hpp"
#include "OrbitTrail.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

// Holds the GPU trail ring against the Kepler solver it was filled from.
// Nothing is drawn: the ring is copied back and every slot is compared with the
// position that slot is supposed to hold.
//
// The ring lives in a GPU-only buffer written by staging copies rather than in
// a CPUWrite buffer, because CPUWrite storage is multiplexed across
// RHI_FRAMES_IN_FLIGHT physical slots and an incremental append would land in
// one of three. This check rotates the frame index between probes precisely so
// that mistake cannot pass.

namespace{
    using namespace Crowy;

    constexpr f64 START_DAY = 0.0;
    // a memcpy chain should reproduce the samples exactly; the slack is only
    // for older batches, whose day was computed against a different endDay and
    // can differ by an ulp before it is narrowed to f32
    constexpr f32 TOLERANCE_AU = 1e-4f;

    // deliberately not a power of two, and not a multiple of the tick counts
    // below: a ring that only ever wraps on nice boundaries hides bugs
    constexpr u32 SMALL_CAPACITY = 37;

    // The compute path solves in float where the CPU solves in double, so it
    // cannot be held to equality. A whole ring of fixed-point phase drifts
    // 1.6e-5 degrees (see KeplerPhase.ReconstructsMeanAnomaly) and float Newton
    // plus six trigonometric functions add about a part in a million - at
    // Neptune that is tens of microns of an AU.
    constexpr f32 GPU_TOLERANCE_AU = 1e-4f;

    f32 Delta(Vec3 lhs, Vec3 rhs){
        return std::max({
            std::abs(lhs.x - rhs.x),
            std::abs(lhs.y - rhs.y),
            std::abs(lhs.z - rhs.z)
        });
    }

    // One frame's worth of work, fully serialized: push, copy, read back, wait.
    //
    // Submit signals ++frameIndex, so getting back to the physical slot the
    // copy was recorded against takes another RHI_FRAMES_IN_FLIGHT - 1 - the
    // same nudge the other headless checks make. The extra step after the
    // download rotates the staging slot, which is the point of the exercise.
    class RingProbe{
    private:
        RHIDevice& device;
        RHICommandListRAII cmdList;
        RHIBufferRAII readback;
        u32 ringBytes;

    public:
        RingProbe(RHIDevice& device, u32 capacity)
            : device(device)
            , cmdList(device.CreateCommandList())
            , ringBytes(capacity * ORBIT_SAMPLE_BYTES)
        {
            readback = device.CreateBuffer(RHIBufferCreateDesc{
                .size = ringBytes,
                .usage = RHIBufferUsage::CopyDst,
                .access = RHIMemoryAccess::CPURead
            }, "OrbitTrailReadback");
        }

        void Flush(OrbitTrail& trail, std::span<Vec3> out){
            CROWY_ASSERT(out.size_bytes() == ringBytes);

            cmdList->Begin();

            const auto edge = trail.Record(*cmdList);

            // the acquire exists only when something was written; a frame that
            // pushed nothing leaves the ring resting in CopySrc already
            std::vector<RHIBufferBarrier> acquires;
            if(edge.has_value())
                acquires.push_back(*edge);

            cmdList->BeginBlitPass({}, acquires);
            cmdList->Copy(trail.Buffer(), *readback, 0, 0, ringBytes);
            cmdList->EndBlitPass();

            cmdList->Close();

            RHICommandList* lists[] = {cmdList.get()};
            device.Submit(lists);
            device.WaitFrame(device.GetSubmittedFrame());

            device.GetFrameIndexRef() += RHI_FRAMES_IN_FLIGHT - 1;
            readback->Download(out.data(), ringBytes);

            // next probe records against the next physical slot
            device.GetFrameIndexRef() += 1;
        }
    };

    struct Mismatch{
        u32 count = 0;
        f32 worst = 0.0f;
        u32 worstAge = 0;
        u32 worstBody = 0;
    };

    Mismatch Compare(const OrbitTrail& trail, std::span<const Vec3> ring){
        Mismatch result;

        std::array<Vec3d, ORBIT_BODY_COUNT> expected;
        for(u32 age=0; age<trail.Filled(); ++age){
            SampleOrbits(trail.DayForAge(age), expected);

            const auto base =
                static_cast<usize>(trail.SlotForAge(age)) * ORBIT_BODY_COUNT;
            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                const auto delta = Delta(
                    ring[base + b],
                    static_cast<Vec3>(expected[b])
                );
                if(delta > result.worst){
                    result.worst = delta;
                    result.worstAge = age;
                    result.worstBody = b;
                }
                if(delta > TOLERANCE_AU)
                    ++result.count;
            }
        }

        return result;
    }

    bool Report(CStr label, const OrbitTrail& trail, const Mismatch& m){
        std::println("  {}: filled {}, head {}, worst delta {:.3e} AU",
            label, trail.Filled(), trail.Head(), m.worst
        );

        if(m.count == 0)
            return true;

        std::println(
            "  FAIL: {} of {} values disagree; worst at age {} ({}), "
            "which should hold day {:.4f}",
            m.count, trail.Filled() * ORBIT_BODY_COUNT,
            m.worstAge, ORBIT_BODY_NAMES[m.worstBody],
            trail.DayForAge(m.worstAge)
        );

        return false;
    }

    // The prefill is one contiguous write of the whole ring, and every sample's
    // day is the same expression DayForAge reports, so this one has to be
    // bit-exact - a nonzero worst delta already means something is off.
    bool CheckPrefill(RHIDevice& device){
        std::println("prefill, full capacity ({} samples)", ORBIT_TRAIL_CAPACITY);

        OrbitTrail trail(
            device, 1.0, ORBIT_TRAIL_CAPACITY, RHIResourceUsage::CopySrc
        );
        RingProbe probe(device, ORBIT_TRAIL_CAPACITY);

        std::vector<Vec3> ring(
            static_cast<usize>(ORBIT_TRAIL_CAPACITY) * ORBIT_BODY_COUNT
        );

        // the hitch Step 7 is measured against: 65536 * 9 Kepler solves plus
        // the memcpy into staging, all on the CPU
        const auto before = std::chrono::steady_clock::now();
        trail.Prefill(START_DAY);
        const auto prefillMs = std::chrono::duration<f64, std::milli>(
            std::chrono::steady_clock::now() - before
        ).count();

        probe.Flush(trail, ring);

        std::println("  CPU prefill: {:.1f} ms", prefillMs);
        std::println("  copies this frame: {}", trail.Stats().copyCount);
        if(trail.Stats().copyCount != 1){
            std::println(
                "  FAIL: a prefill starts at slot 0 and should be one copy, not {}",
                trail.Stats().copyCount
            );

            return false;
        }
        if(trail.Filled() != ORBIT_TRAIL_CAPACITY){
            std::println("  FAIL: prefill left the ring only {} deep", trail.Filled());

            return false;
        }

        const auto m = Compare(trail, ring);
        if(m.worst != 0.0f){
            std::println(
                "  FAIL: prefill should reproduce exactly, worst delta {:.3e} AU",
                m.worst
            );

            return false;
        }

        return Report("prefill", trail, m);
    }

    // A ring this small wraps every few frames, so the two-run copy and the
    // modulo indexing both get exercised many times over.
    bool CheckWraparound(RHIDevice& device){
        constexpr u32 FRAMES = 24;
        constexpr f64 FRAME_SECONDS = 1.0 / 60.0;
        // 5 ticks a frame against a 37-slot ring: the seam lands on a different
        // offset every lap
        constexpr f64 DAYS_PER_SECOND = 5.0 * 60.0;

        std::println("wraparound, {} slots, {} frames", SMALL_CAPACITY, FRAMES);

        OrbitTrail trail(device, 1.0, SMALL_CAPACITY, RHIResourceUsage::CopySrc);
        RingProbe probe(device, SMALL_CAPACITY);

        std::vector<Vec3> ring(
            static_cast<usize>(SMALL_CAPACITY) * ORBIT_BODY_COUNT
        );

        trail.Prefill(START_DAY);
        probe.Flush(trail, ring);

        bool ok = true;
        f32 worst = 0.0f;
        for(u32 frame=0; frame<FRAMES; ++frame){
            const auto headBefore = trail.Head();
            const auto ticks = trail.Advance(FRAME_SECONDS, DAYS_PER_SECOND);
            probe.Flush(trail, ring);

            // sample-major means a tick run is contiguous: one copy, and two
            // only where it runs off the end of the ring
            const auto expectedCopies = ticks == 0 ?
                0u :
                (headBefore + ticks > SMALL_CAPACITY ? 2u : 1u);
            if(trail.Stats().copyCount != expectedCopies){
                std::println(
                    "  FAIL: frame {} wrote {} samples from slot {} in {} copies, "
                    "expected {}",
                    frame, ticks, headBefore,
                    trail.Stats().copyCount, expectedCopies
                );
                ok = false;

                break;
            }

            const auto m = Compare(trail, ring);
            worst = std::max(worst, m.worst);
            if(m.count > 0){
                std::println("  frame {} (head {}, {} copies)",
                    frame, trail.Head(), trail.Stats().copyCount
                );
                Report("wraparound", trail, m);
                ok = false;

                break;
            }
        }

        const auto& stats = trail.Stats();
        std::println(
            "  ticks {}, frames needing a split copy: {}, worst delta {:.3e} AU",
            stats.totalTicks, stats.splitFrameCount, worst
        );

        if(stats.splitFrameCount == 0){
            std::println(
                "  FAIL: the ring never wrapped - this check proved nothing"
            );
            ok = false;
        }

        return ok;
    }

    // The sim step is fixed, so the same wall-clock span has to advance sim time
    // by the same amount however it is chopped into frames.
    bool CheckFixedTimestep(RHIDevice& device){
        constexpr f64 TOTAL_SECONDS = 2.0;
        constexpr f64 DAYS_PER_SECOND = 37.0;

        std::println("fixed timestep, {} sim days/s", DAYS_PER_SECOND);

        const auto run = [&](u32 frames){
            OrbitTrail trail(
                device, 1.0, SMALL_CAPACITY, RHIResourceUsage::CopySrc
            );
            trail.Prefill(START_DAY);

            RingProbe probe(device, SMALL_CAPACITY);
            std::vector<Vec3> ring(
                static_cast<usize>(SMALL_CAPACITY) * ORBIT_BODY_COUNT
            );
            probe.Flush(trail, ring);

            for(u32 f=0; f<frames; ++f){
                trail.Advance(TOTAL_SECONDS / frames, DAYS_PER_SECOND);
                probe.Flush(trail, ring);
            }

            return std::pair{trail.NewestDay(), trail.Stats().totalTicks};
        };

        const auto [daySteady, ticksSteady] = run(60);
        const auto [dayStuttering, ticksStuttering] = run(7);

        std::println("  60 frames -> day {:.3f} ({} ticks)", daySteady, ticksSteady);
        std::println("   7 frames -> day {:.3f} ({} ticks)", dayStuttering, ticksStuttering);

        // the accumulator can be holding a partial tick at the end, so one
        // tick of disagreement is the floor, not a defect
        if(std::abs(daySteady - dayStuttering) > 1.0){
            std::println(
                "  FAIL: frame pacing changed how far sim time advanced"
            );

            return false;
        }

        return true;
    }

    // A timeScale nobody can keep up with must slow sim time down and say so,
    // not stall the frame trying to catch up.
    bool CheckTickOverflow(RHIDevice& device){
        std::println("tick overflow");

        OrbitTrail trail(device, 1.0, SMALL_CAPACITY, RHIResourceUsage::CopySrc);
        RingProbe probe(device, SMALL_CAPACITY);
        std::vector<Vec3> ring(
            static_cast<usize>(SMALL_CAPACITY) * ORBIT_BODY_COUNT
        );

        trail.Prefill(START_DAY);
        probe.Flush(trail, ring);

        const auto ticks = trail.Advance(1.0, 100000.0);
        probe.Flush(trail, ring);

        const auto& stats = trail.Stats();
        std::println("  pushed {} ticks, dropped {}, overflow frames {}",
            ticks, stats.droppedTicks, stats.overflowFrameCount
        );

        const auto cap = std::min(ORBIT_MAX_TICKS_PER_FRAME, SMALL_CAPACITY);
        if(ticks != cap){
            std::println("  FAIL: expected the {}-tick cap to engage", cap);

            return false;
        }
        if(stats.overflowFrameCount == 0 || stats.droppedTicks == 0){
            std::println("  FAIL: ticks were dropped without being counted");

            return false;
        }

        const auto m = Compare(trail, ring);

        return Report("after overflow", trail, m);
    }

    // Same ring, filled both ways, held against each other rather than against
    // the solver: this is the only check that can catch the compute path
    // disagreeing with the CPU it is replacing.
    bool CheckGpuMatchesCpu(RHIDevice& device){
        constexpr u32 FRAMES = 24;
        constexpr f64 FRAME_SECONDS = 1.0 / 60.0;
        constexpr f64 DAYS_PER_SECOND = 5.0 * 60.0;

        std::println("cpu vs gpu fill, {} slots, {} frames",
            SMALL_CAPACITY, FRAMES
        );

        OrbitTrail cpu(device, 1.0, SMALL_CAPACITY, RHIResourceUsage::CopySrc);
        OrbitTrail gpu(device, 1.0, SMALL_CAPACITY, RHIResourceUsage::CopySrc);
        gpu.SetFillMode(OrbitFillMode::Gpu);

        RingProbe cpuProbe(device, SMALL_CAPACITY);
        RingProbe gpuProbe(device, SMALL_CAPACITY);

        const usize ringSize =
            static_cast<usize>(SMALL_CAPACITY) * ORBIT_BODY_COUNT;
        std::vector<Vec3> cpuRing(ringSize), gpuRing(ringSize);

        f32 worst = 0.0f;
        u32 worstAge = 0, worstBody = 0;

        const auto compare = [&](u32 frame){
            for(u32 age=0; age<cpu.Filled(); ++age){
                const auto base =
                    static_cast<usize>(cpu.SlotForAge(age)) * ORBIT_BODY_COUNT;
                for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                    const auto delta = Delta(cpuRing[base + b], gpuRing[base + b]);
                    if(delta > worst){
                        worst = delta;
                        worstAge = age;
                        worstBody = b;
                    }
                }
            }

            if(worst <= GPU_TOLERANCE_AU)
                return true;

            std::println(
                "  FAIL: frame {} disagrees by {:.3e} AU at age {} ({})",
                frame, worst, worstAge, ORBIT_BODY_NAMES[worstBody]
            );

            return false;
        };

        cpu.Prefill(START_DAY);
        gpu.Prefill(START_DAY);
        cpuProbe.Flush(cpu, cpuRing);
        gpuProbe.Flush(gpu, gpuRing);

        if(gpu.Stats().dispatchCount != 1){
            std::println("  FAIL: the GPU prefill recorded {} dispatches, not 1",
                gpu.Stats().dispatchCount
            );

            return false;
        }
        if(gpu.Stats().copyCount != 0){
            std::println("  FAIL: the GPU path still issued {} copies",
                gpu.Stats().copyCount
            );

            return false;
        }
        if(!compare(0))
            return false;

        for(u32 frame=1; frame<=FRAMES; ++frame){
            cpu.Advance(FRAME_SECONDS, DAYS_PER_SECOND);
            gpu.Advance(FRAME_SECONDS, DAYS_PER_SECOND);
            cpuProbe.Flush(cpu, cpuRing);
            gpuProbe.Flush(gpu, gpuRing);

            if(cpu.Head() != gpu.Head()){
                std::println("  FAIL: frame {} left the heads at {} and {}",
                    frame, cpu.Head(), gpu.Head()
                );

                return false;
            }
            if(!compare(frame))
                return false;
        }

        std::println(
            "  ticks {}, split frames {}, worst delta {:.3e} AU at age {} ({})",
            gpu.Stats().totalTicks, gpu.Stats().splitFrameCount,
            worst, worstAge, ORBIT_BODY_NAMES[worstBody]
        );

        if(gpu.Stats().splitFrameCount == 0){
            std::println("  FAIL: the ring never wrapped on the GPU path");

            return false;
        }

        return true;
    }

    // What the step is for: the same 65536 * 9 solves, off the CPU. The number
    // to compare is the CPU prefill printed above.
    bool CheckGpuPrefill(RHIDevice& device){
        std::println("gpu prefill, full capacity ({} samples)",
            ORBIT_TRAIL_CAPACITY
        );

        OrbitTrail trail(
            device, 1.0, ORBIT_TRAIL_CAPACITY, RHIResourceUsage::CopySrc
        );
        trail.SetFillMode(OrbitFillMode::Gpu);

        RingProbe probe(device, ORBIT_TRAIL_CAPACITY);
        std::vector<Vec3> ring(
            static_cast<usize>(ORBIT_TRAIL_CAPACITY) * ORBIT_BODY_COUNT
        );

        // all that is left on this side is nine phase epochs and a 144-byte
        // upload; Newton and the rotations have gone
        const auto before = std::chrono::steady_clock::now();
        trail.Prefill(START_DAY);
        const auto prefillMs = std::chrono::duration<f64, std::milli>(
            std::chrono::steady_clock::now() - before
        ).count();

        probe.Flush(trail, ring);

        std::println("  CPU cost of a GPU prefill: {:.3f} ms", prefillMs);

        const auto m = Compare(trail, ring);
        std::println("  worst delta against the double solver: {:.3e} AU", m.worst);

        if(m.worst > GPU_TOLERANCE_AU){
            std::println(
                "  FAIL: the compute solve drifted past {:.0e} AU",
                GPU_TOLERANCE_AU
            );

            return false;
        }

        return true;
    }

    // The draw arguments the GPU writes for itself, read back and held against
    // the same formula in double. This is the only thing that can catch the
    // compute pass and the CPU disagreeing about how long a trail is - the
    // picture cannot, because a wrong count looks like a slightly shorter
    // trail and nothing else.
    bool CheckDrawArgs(RHIDevice& device){
        std::println("indirect draw arguments");

        std::array<OrbitBodyDraw, ORBIT_BODY_COUNT> table{};
        for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
            table[b].trailPeriodDays = static_cast<f32>(TrailPeriodDays(b));
        }

        OrbitTrailArgs args(device, table, RHIResourceUsage::CopySrc);

        auto cmdList = device.CreateCommandList();

        const u32 argsBytes = ORBIT_BODY_COUNT *
            static_cast<u32>(sizeof(RHIDrawArgs));
        auto readback = device.CreateBuffer(RHIBufferCreateDesc{
            .size = argsBytes,
            .usage = RHIBufferUsage::CopyDst,
            .access = RHIMemoryAccess::CPURead
        }, "OrbitArgsReadback");

        struct Case{
            u32 filled;
            f32 orbitTurns;
            u32 enabledMask;
            CStr what;
        };
        // the ends of every control, plus a mask with holes in it
        const std::array<Case, 6> cases{
            Case{65536, 1.00f, 0x1FFu, "full ring, one turn"},
            Case{65536, 15.0f, 0x1FFu, "full ring, fifteen turns - Neptune clamps"},
            Case{65536, 0.05f, 0x1FFu, "full ring, shortest trail"},
            Case{  120, 1.00f, 0x1FFu, "barely filled - everything clamps"},
            Case{65536, 1.00f, 0x000u, "everything switched off"},
            Case{65536, 1.00f, 0x155u, "alternating bodies"}
        };

        bool ok = true;
        for(const auto& c: cases){
            cmdList->Begin();
            const auto edges = args.Record(
                *cmdList, c.filled, c.orbitTurns, 1.0f, c.enabledMask
            );

            const std::array acquires{edges.args};
            cmdList->BeginBlitPass({}, acquires);
            cmdList->Copy(args.Args(), *readback, 0, 0, argsBytes);
            cmdList->EndBlitPass();
            cmdList->Close();

            RHICommandList* lists[] = {cmdList.get()};
            device.Submit(lists);
            device.WaitFrame(device.GetSubmittedFrame());

            device.GetFrameIndexRef() += RHI_FRAMES_IN_FLIGHT - 1;
            std::array<RHIDrawArgs, ORBIT_BODY_COUNT> got{};
            readback->Download(got.data(), argsBytes);
            device.GetFrameIndexRef() += 1;

            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                // the same expression the CPU used to run every frame
                const auto wanted = static_cast<f64>(c.orbitTurns) *
                    TrailPeriodDays(b);
                const auto usable = std::min(
                    c.filled,
                    static_cast<u32>(std::min(
                        wanted, static_cast<f64>(c.filled)
                    ))
                );
                const bool enabled = (c.enabledMask & (1u << b)) != 0;
                const u32 expected = (enabled && usable > 1) ? usable - 1 : 0;

                if(got[b].vertexCount != 4 || got[b].firstVertex != 0){
                    std::println(
                        "  FAIL: {} - {} got vertexCount {} firstVertex {}",
                        c.what, ORBIT_BODY_NAMES[b],
                        got[b].vertexCount, got[b].firstVertex
                    );
                    ok = false;
                }
                // baseInstance is the drawID the vertex stage reads as bodyIdx;
                // getting this wrong recolours every trail
                if(got[b].baseInstance != b){
                    std::println("  FAIL: {} - {} got baseInstance {}, not {}",
                        c.what, ORBIT_BODY_NAMES[b], got[b].baseInstance, b
                    );
                    ok = false;
                }
                // The shader works in float where this works in double, so the
                // truncation to a whole sample could in principle land either
                // side of a boundary. Over every extreme the panel allows it
                // never does, so this demands equality: a drifting formula is
                // worth a failure, not a shrug.
                const auto delta = static_cast<i64>(got[b].instanceCount) -
                    static_cast<i64>(expected);
                if(delta != 0){
                    std::println(
                        "  FAIL: {} - {} got {} instances, expected {}",
                        c.what, ORBIT_BODY_NAMES[b],
                        got[b].instanceCount, expected
                    );
                    ok = false;
                }
                if(!enabled && got[b].instanceCount != 0){
                    std::println(
                        "  FAIL: {} - {} is switched off but draws {}",
                        c.what, ORBIT_BODY_NAMES[b], got[b].instanceCount
                    );
                    ok = false;
                }
            }

            u64 total = 0;
            for(const auto& a: got)
                total += a.instanceCount;
            std::println("  {}: {} instances over {} draws",
                c.what, total, ORBIT_BODY_COUNT
            );
        }

        return ok;
    }
}

int main(void){
    try{
        using namespace Crowy;

        auto device = CreateDevice();

        bool ok = true;
        ok = CheckPrefill(*device) && ok;
        ok = CheckWraparound(*device) && ok;
        ok = CheckFixedTimestep(*device) && ok;
        ok = CheckTickOverflow(*device) && ok;
        ok = CheckGpuMatchesCpu(*device) && ok;
        ok = CheckGpuPrefill(*device) && ok;
        ok = CheckDrawArgs(*device) && ok;

        if(!ok)
            return 1;

        std::println("Succeed!");
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
