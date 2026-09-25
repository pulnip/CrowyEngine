#include "FrameControl.hpp"

#include <algorithm>

namespace Crowy
{
    void FrameControl::Run() noexcept {
        mode = Mode::Free;
    }

    void FrameControl::Hold() noexcept {
        mode = Mode::Held;
    }

    void FrameControl::RunFrames(u64 count) noexcept {
        mode = Mode::Frames;
        target = count;
    }

    void FrameControl::RunUntil(u64 frame) noexcept {
        mode = Mode::Until;
        target = frame;
    }

    FrameControl::Gate FrameControl::Advance(u64 lastFrame) noexcept {
        if(mode == Mode::Free)
            return Gate::Free;

        // a counted run that has reached its frame becomes a plain hold
        if(IsHeld(lastFrame)) {
            mode = Mode::Held;
            return Gate::Hold;
        }

        if(mode == Mode::Frames)
            --target;
        return Gate::Step;
    }

    bool FrameControl::IsHeld(u64 lastFrame) const noexcept {
        return HoldAt(lastFrame) == lastFrame;
    }

    std::optional<u64> FrameControl::HoldAt(u64 lastFrame) const noexcept {
        using enum Mode;

        switch(mode) {
        case Free:
            return std::nullopt;
        case Held:
            return lastFrame;
        case Frames:
            return lastFrame + target;
        case Until:
            // a frame already passed holds at once
            return std::max(target, lastFrame);
        }
        return std::nullopt;
    }
}
