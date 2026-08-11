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
        Frame,      // the whole iteration
        // Sentinel
        Unknown
    };
    constexpr auto NUM_FRAME_SECTION = static_cast<usize>(FrameSection::Unknown);

    CStr ToString(FrameSection) noexcept;

#if CROWY_BENCHMARK
    // Times each section of the frame loop, keeps every sample, and writes
    // percentiles out at the end. Nothing is printed while running: a
    // benchmark is read afterwards, not watched.
    class FrameProfiler{
    private:
        using Clock = std::chrono::steady_clock;

        struct FrameRecord{
            u64 frameNumber = 0;
            std::array<f64, NUM_FRAME_SECTION> sections{};
            RHIFrameStats stats;
        };

        BenchmarkConfig config;
        // for the report header, so a stray file still says what produced it
        Str title;
        u32 width = 0, height = 0;
        bool vsync = true;

        u64 frameNumber = 0;
        std::vector<FrameRecord> records;

        std::array<f64, NUM_FRAME_SECTION> current{};
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

        void BeginFrame() noexcept;
        void EndFrame(const RHIFrameStats&, f64 fenceWaitSeconds) noexcept;

        // true once the measured window is full, so the loop can leave
        // through its normal shutdown instead of dying where it stands
        bool ShouldStop() const noexcept;

        void WriteReport() const;

    private:
        void Accumulate(FrameSection, f64 seconds) noexcept;

        bool IsMeasuring() const noexcept{
            return config.enabled && frameNumber >= config.warmupFrames;
        }
    };
#else
    // Not a benchmark build: every one of these folds to nothing.
    class FrameProfiler{
    public:
        class Scope{
        public:
            constexpr Scope(FrameProfiler&, FrameSection) noexcept{}
        };

        constexpr explicit FrameProfiler(const RuntimeConfig&) noexcept{}

        constexpr void BeginFrame() noexcept{}
        constexpr void EndFrame(const RHIFrameStats&, f64) noexcept{}
        constexpr bool ShouldStop() const noexcept{ return false; }
        constexpr void WriteReport() const noexcept{}
    };
#endif
}
