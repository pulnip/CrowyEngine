#include "RHIFrameTimeline.hpp"

#include "Assert.hpp"

namespace Crowy
{
    void RHIFrameTimeline::OnSubmit(
        u64 frame,
        u64 serial,
        u64 completedSerial
    ) {
        CROWY_ASSERT(
            frame == lastFrame + 1,
            "frame {} submitted after frame {}: frames are contiguous from 1, "
            "and a signal that is not a frame takes no number",
            frame,
            lastFrame
        );

        auto& entry = entries[frame % RHI_FRAMES_IN_FLIGHT];
        CROWY_ASSERT(
            entry.serial <= completedSerial,
            "frame {} submitted while frame {} is still on the GPU: frame N "
            "waits for frame N - {} first",
            frame,
            entry.frame,
            RHI_FRAMES_IN_FLIGHT
        );

        entry = Entry{.frame = frame, .serial = serial};
        lastFrame = frame;
    }

    std::optional<u64> RHIFrameTimeline::SerialOf(u64 frame) const {
        CROWY_ASSERT(
            frame <= lastFrame,
            "frame {} has not been submitted; the last one is {}",
            frame,
            lastFrame
        );

        if(frame == 0 || frame + RHI_FRAMES_IN_FLIGHT <= lastFrame)
            return std::nullopt;

        const auto& entry = entries[frame % RHI_FRAMES_IN_FLIGHT];
        CROWY_ASSERT(entry.frame == frame);
        return entry.serial;
    }

    u64 RHIFrameTimeline::CompletedFrame(u64 completedSerial) const {
        // serials grow with frames, so the newest completed one is the first
        // found walking back through the ring
        for(u64 frame = lastFrame;
            frame > 0 && frame + RHI_FRAMES_IN_FLIGHT > lastFrame;
            --frame) {
            if(entries[frame % RHI_FRAMES_IN_FLIGHT].serial <= completedSerial)
                return frame;
        }

        // nothing in the ring has completed, but the frame before it has
        return lastFrame > RHI_FRAMES_IN_FLIGHT
                   ? lastFrame - RHI_FRAMES_IN_FLIGHT
                   : 0;
    }
}
