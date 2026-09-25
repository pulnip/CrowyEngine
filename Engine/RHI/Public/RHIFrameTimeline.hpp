#pragma once

#include <array>
#include <optional>

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // Which serial each in-flight frame's submission signaled. Frame N is
    // submitted only once frame N - RHI_FRAMES_IN_FLIGHT has completed, so a
    // frame older than the ring is complete by that contract alone.
    class RHIFrameTimeline {
    private:
        struct Entry {
            u64 frame = 0;
            u64 serial = 0;
        };

        std::array<Entry, RHI_FRAMES_IN_FLIGHT> entries{};
        u64 lastFrame = 0;

    public:
        // asserts the contract: frames arrive contiguously from 1, and the
        // frame whose entry this overwrites has completed
        void OnSubmit(u64 frame, u64 serial, u64 completedSerial);

        // the serial to wait on for `frame`; none when the frame is already
        // complete by the contract (frame 0, or older than the ring)
        std::optional<u64> SerialOf(u64 frame) const;

        // the newest submitted frame whose serial has completed; 0 before any
        u64 CompletedFrame(u64 completedSerial) const;
    };
}
