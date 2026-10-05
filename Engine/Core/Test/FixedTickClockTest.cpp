#include <array>
#include <chrono>

#include <gtest/gtest.h>

#include "FixedTickClock.hpp"

using namespace Crowy;

namespace
{
    // what Timer hands a frame: whole nanoseconds as f64 seconds
    f64 asTimerSeconds(i64 nanoseconds) {
        return std::chrono::duration<f64>(std::chrono::nanoseconds(nanoseconds))
            .count();
    }
}

TEST(FixedTickClock, OneFixedStepIsOneTick) {
    FixedTickClock clock;
    EXPECT_EQ(clock.Advance(16'666'667), 1u);
    EXPECT_EQ(clock.Carry(), 0);
}

// a counted port frame is one tick whatever free frames left behind
TEST(FixedTickClock, FixedStepsKeepTheCarry) {
    FixedTickClock clock;
    EXPECT_EQ(clock.Advance(16'666'666), 0u);
    for(u32 frame = 0; frame < 600; ++frame) {
        ASSERT_EQ(clock.Advance(TickNanoseconds), 1u);
        ASSERT_EQ(clock.Carry(), 16'666'666);
    }
}

TEST(FixedTickClock, HundredTwentyHertzTicksEveryOtherFrame) {
    FixedTickClock clock;
    EXPECT_EQ(clock.Advance(8'333'333), 0u);
    EXPECT_EQ(clock.Advance(8'333'333), 0u);
    EXPECT_EQ(clock.Carry(), 16'666'666);
    EXPECT_EQ(clock.Advance(8'333'333), 1u);
    EXPECT_EQ(clock.Carry(), 8'333'332);

    u32 ticks = 1;
    for(u32 frame = 3; frame < 120; ++frame)
        ticks += clock.Advance(8'333'333);
    EXPECT_EQ(ticks, 59u);
    EXPECT_EQ(clock.Carry(), 16'666'607);

    FixedTickClock faster;
    u32 fasterTicks = 0;
    for(u32 frame = 0; frame < 120; ++frame)
        fasterTicks += faster.Advance(8'333'334);
    EXPECT_EQ(fasterTicks, 60u);
    EXPECT_EQ(faster.Carry(), 60);
}

TEST(FixedTickClock, LongFrameIsClampedAndKeepsItsFraction) {
    FixedTickClock clock;
    EXPECT_EQ(clock.Advance(1'000'000'000), MaxTicksPerAdvance);
    EXPECT_EQ(clock.Carry(), 16'666'647);

    FixedTickClock other;
    EXPECT_EQ(other.Advance(5 * TickNanoseconds + 123), 4u);
    EXPECT_EQ(other.Carry(), 123);
    EXPECT_EQ(other.Advance(TickNanoseconds - 123), 1u);
    EXPECT_EQ(other.Carry(), 0);

    FixedTickClock exact;
    EXPECT_EQ(exact.Advance(4 * TickNanoseconds), 4u);
    EXPECT_EQ(exact.Carry(), 0);
    EXPECT_EQ(exact.Advance(5 * TickNanoseconds - 1), 4u);
    EXPECT_EQ(exact.Carry(), TickNanoseconds - 1);
}

TEST(FixedTickClock, NothingOrBackwardsIsNoTick) {
    FixedTickClock clock;
    EXPECT_EQ(clock.Advance(0), 0u);
    EXPECT_EQ(clock.Advance(-5), 0u);
    EXPECT_EQ(clock.Carry(), 0);
    EXPECT_EQ(toNanoseconds(-0.001), 0);
}

TEST(FixedTickClock, TimerSecondsComeBackAsTheirNanoseconds) {
    constexpr std::array<i64, 4> Frames{
        1,
        8'333'333,
        16'666'667,
        1'000'000'000,
    };
    for(const auto nanoseconds: Frames)
        EXPECT_EQ(toNanoseconds(asTimerSeconds(nanoseconds)), nanoseconds);

    u32 checked = 0;
    for(i64 k = 0; k < 1000; ++k) {
        const auto nanoseconds = k * 9'999'991;
        ASSERT_EQ(toNanoseconds(asTimerSeconds(nanoseconds)), nanoseconds);
        ++checked;
    }
    EXPECT_EQ(checked, 1000u);
}
