#pragma once

#include <optional>

#include "Primitives.hpp"

namespace Crowy
{
    // Whether the loop may begin its next frame. Consulted once between
    // frames with the frame that ended last; free-running until told
    // otherwise.
    class FrameControl {
    public:
        // Step: let through by a counted run, so the frame advances time by
        // a fixed step
        enum class Gate : u8 { Hold, Free, Step };

    private:
        enum class Mode : u8 { Free, Held, Frames, Until };

        Mode mode = Mode::Free;
        // Frames: how many more may begin; Until: the frame to hold after
        u64 target = 0;

    public:
        void Run() noexcept;
        void Hold() noexcept;
        // count more frames, then held
        void RunFrames(u64 count) noexcept;
        // until `frame` has ended, then held
        void RunUntil(u64 frame) noexcept;

        // consumes one frame of a counted run
        Gate Advance(u64 lastFrame) noexcept;
        // no further frame begins until told: held, or a counted run whose
        // last frame has ended
        bool IsHeld(u64 lastFrame) const noexcept;
        // the frame it will hold after; none while free
        std::optional<u64> HoldAt(u64 lastFrame) const noexcept;
    };
}
