#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <print>
#include <span>
#include <vector>
#include "Kepler.hpp"
#include "OrbitTrail.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"

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
        RHIFenceRAII fence;
        RHIBufferRAII readback;
        u32 ringBytes;

    public:
        RingProbe(RHIDevice& device, u32 capacity)
            : device(device)
            , cmdList(device.CreateCommandList())
            , fence(device.CreateFence())
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
            device.Submit(lists, *fence);
            fence->WaitCPU(device.GetFrameIndexRef());

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
