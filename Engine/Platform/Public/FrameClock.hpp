#pragma once

#include "FrameControl.hpp"
#include "Primitives.hpp"
#include "Timer.hpp"

namespace Crowy
{
    // Time and the hold gate of one loop: when a frame may begin, what it
    // took, and whether the loop was asked to leave.
    class FrameClock {
    private:
        Timer timer;
        FrameControl control;
        // the pacer's frame: the one being recorded, or between frames the
        // last one that ended
        u64 frame = 0;
        // the gap before a frame - a hold, or init before frame 1 - is not
        // that frame's time
        bool held = true;
        bool stepped = false;
        bool quitRequested = false;
        // every frame takes the fixed step, so a frame capture's frame N is
        // one picture
        bool fixedSteps = false;

    public:
        FrameClock() = default;
        explicit FrameClock(bool fixedSteps) noexcept
            : fixedSteps(fixedSteps) {}

        // false holds the loop; a quit releases any hold, even one that
        // arrives after it
        bool Advance();
        void BeginFrame(u64 frame) noexcept { this->frame = frame; }
        // the frame's time: a fixed step if a counted run let it through
        void Tick() noexcept;
        void RequestQuit() noexcept { quitRequested = true; }

        FrameControl& Control() noexcept { return control; }
        const FrameControl& Control() const noexcept { return control; }
        u64 Frame() const noexcept { return frame; }
        bool QuitRequested() const noexcept { return quitRequested; }
        f64 DeltaTime() const noexcept { return timer.GetDeltaTime(); }
        // every frame's time so far, with no hold in it
        f64 ElapsedTime() const noexcept { return timer.GetElapsedTime(); }
    };
}
