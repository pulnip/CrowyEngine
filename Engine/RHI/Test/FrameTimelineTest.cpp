#include <array>

#include <gtest/gtest.h>

#include "RHIFrameTimeline.hpp"

using namespace Crowy;

namespace
{
    constexpr u64 Depth = RHI_FRAMES_IN_FLIGHT;

    // the serial each frame signals: out-of-band signals after frames 2 and 4
    // take 4 and 7, so serials and frames part the way a resize parts them
    constexpr std::array<u64, 6> FrameSerials{0, 2, 3, 5, 6, 8};

    // frames 1..count, each submitted once the one it overwrites completed
    RHIFrameTimeline submitted(u64 count) {
        RHIFrameTimeline timeline;
        for(u64 frame = 1; frame <= count; ++frame) {
            const auto overwritten = frame > Depth ? FrameSerials[frame - Depth] : 0;
            timeline.OnSubmit(frame, FrameSerials[frame], overwritten);
        }
        return timeline;
    }
}

TEST(RHIFrameTimeline, NothingSubmittedIsFrameZero) {
    const RHIFrameTimeline timeline;

    EXPECT_EQ(timeline.CompletedFrame(0), 0u);
    EXPECT_EQ(timeline.CompletedFrame(100), 0u);
    EXPECT_FALSE(timeline.SerialOf(0).has_value());
}

TEST(RHIFrameTimeline, SerialOfNamesTheFramesSignalInsideTheRing) {
    const auto timeline = submitted(5);

    EXPECT_EQ(timeline.SerialOf(5), 8u);
    EXPECT_EQ(timeline.SerialOf(4), 6u);
    EXPECT_EQ(timeline.SerialOf(3), 5u);
}

TEST(RHIFrameTimeline, FramesOlderThanTheRingAreCompleteByContract) {
    const auto timeline = submitted(5);

    EXPECT_FALSE(timeline.SerialOf(2).has_value());
    EXPECT_FALSE(timeline.SerialOf(1).has_value());
    EXPECT_FALSE(timeline.SerialOf(0).has_value());
}

TEST(RHIFrameTimeline, CompletedFrameIsTheNewestFrameAtOrBelowTheSerial) {
    const auto timeline = submitted(5);

    EXPECT_EQ(timeline.CompletedFrame(8), 5u);
    // the out-of-band 7 completes no frame of its own
    EXPECT_EQ(timeline.CompletedFrame(7), 4u);
    EXPECT_EQ(timeline.CompletedFrame(6), 4u);
    EXPECT_EQ(timeline.CompletedFrame(5), 3u);
    // nothing in the ring yet, but frame 2 finished before frame 5 went out
    EXPECT_EQ(timeline.CompletedFrame(4), 2u);
}

TEST(RHIFrameTimeline, EarlyFramesCountFromZero) {
    const auto timeline = submitted(2);

    EXPECT_EQ(timeline.CompletedFrame(0), 0u);
    EXPECT_EQ(timeline.CompletedFrame(2), 1u);
    EXPECT_EQ(timeline.CompletedFrame(3), 2u);
}

#if defined(_DEBUG) || !defined(NDEBUG)
TEST(RHIFrameTimelineDeathTest, FramesAreContiguous) {
    EXPECT_DEATH(
        {
            RHIFrameTimeline timeline;
            timeline.OnSubmit(1, 1, 0);
            timeline.OnSubmit(3, 2, 0);
        },
        "contiguous"
    );
}

TEST(RHIFrameTimelineDeathTest, FrameNWaitsForFrameNMinusDepth) {
    EXPECT_DEATH(
        {
            RHIFrameTimeline timeline;
            timeline.OnSubmit(1, 1, 0);
            timeline.OnSubmit(2, 2, 0);
            timeline.OnSubmit(3, 3, 0);
            // frame 1's serial 1 has not completed
            timeline.OnSubmit(4, 4, 0);
        },
        "still on the GPU"
    );
}

TEST(RHIFrameTimelineDeathTest, AFutureFrameHasNoSerial) {
    EXPECT_DEATH(
        {
            RHIFrameTimeline timeline;
            timeline.OnSubmit(1, 1, 0);
            (void)timeline.SerialOf(2);
        },
        "has not been submitted"
    );
}
#endif
