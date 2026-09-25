#pragma once

#include <array>
#include <chrono>
#include <vector>
#include "Primitives.hpp"
#include "RHIFrameStats.hpp"
#include "RuntimeConfig.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // The frame loop cut where the cost actually differs between designs.
    // Record is the one the harness exists for: dividing it by the draw
    // count gives the per-draw CPU cost, which is what separates a D3D12
    // backend from a D3D11 one dressed up as D3D12.
    enum class FrameSection{
        Events,     // window messages and input
        Update,     // game logic, identical either way - the control
        FenceWait,  // blocked because the GPU is behind
        Acquire,    // swapchain image and command list setup
        Record,     // draw submission
        Submit,     // execute and present
        Frame,      // the fence wait through the submit
        // Sentinel
        Unknown
    };
    constexpr auto NUM_FRAME_SECTION = static_cast<usize>(FrameSection::Unknown);

    CStr ToString(FrameSection) noexcept;

    // What one loop frame measured, handed to the app as the frame ends.
    // Every build has it; without CROWY_FRAME_STATS it stays empty.
    struct FrameReport{
        // the pacer's frame number, counted from 1
        u64 frame = 0;
        std::array<f64, NUM_FRAME_SECTION> seconds{};
        RHIFrameStats rhi;
    };

#if CROWY_FRAME_STATS
    // Times each section of the frame loop, keeps every sample, and writes
    // percentiles out at the end. Nothing is printed while running: a
    // benchmark is read afterwards, not watched.
    class FrameProfiler{
    private:
        using Clock = std::chrono::steady_clock;

        BenchmarkConfig config;
        // for the report header, so a stray file still says what produced it
        Str title;
        u32 width = 0, height = 0;
        bool vsync = true;

        std::vector<FrameReport> records;

        FrameReport current;
        Clock::time_point frameStart;

    public:
        explicit FrameProfiler(const RuntimeConfig&);
        ~FrameProfiler() = default;
        CROWY_DECLARE_PINNED(FrameProfiler)

        // Times one section for as long as it lives. The frame loop breaks
        // out of the middle, so this cannot be a manual begin/end pair.
        class Scope{
        private:
            FrameProfiler& owner;
            FrameSection section;
            Clock::time_point start;

        public:
            Scope(FrameProfiler& owner, FrameSection section) noexcept
                : owner(owner)
                , section(section)
                , start(Clock::now())
            {}
            ~Scope() noexcept{
                owner.Accumulate(section, std::chrono::duration<f64>(
                    Clock::now() - start
                ).count());
            }
            CROWY_DECLARE_PINNED(Scope)
        };

        // the pacer has already assigned `frame` and waited for its fence
        void BeginFrame(u64 frame, f64 fenceWaitSeconds) noexcept;
        const FrameReport& EndFrame(const RHIFrameStats&) noexcept;

        // true once the measured window is full, so the loop can leave
        // through its normal shutdown instead of dying where it stands
        bool ShouldStop() const noexcept;

        void WriteReport() const;

    private:
        void Accumulate(FrameSection, f64 seconds) noexcept;

        bool IsMeasuring() const noexcept{
            return config.enabled && current.frame > config.warmupFrames;
        }
    };
#else
    // Release without CROWY_BENCHMARK: every one of these folds to nothing.
    class FrameProfiler{
    public:
        class Scope{
        public:
            constexpr Scope(FrameProfiler&, FrameSection) noexcept{}
        };

        constexpr explicit FrameProfiler(const RuntimeConfig&) noexcept{}

        constexpr void BeginFrame(u64, f64) noexcept{}
        const FrameReport& EndFrame(const RHIFrameStats&) noexcept{
            static constexpr FrameReport none;
            return none;
        }
        constexpr bool ShouldStop() const noexcept{ return false; }
        constexpr void WriteReport() const noexcept{}
    };
#endif
}
