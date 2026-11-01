#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "FixedTickClock.hpp"
#include "FrameClock.hpp"

using namespace Crowy;
using namespace std::chrono_literals;

namespace
{
    // well above the few microseconds between two calls, well below the
    // sleeps these tests take
    constexpr f64 NoTime = 0.05;

    void runFrame(FrameClock& clock, u64 frame) {
        ASSERT_TRUE(clock.Advance());
        clock.BeginFrame(frame);
        clock.Tick();
    }
}

TEST(FrameClock, TheGapBeforeFrameOneIsNotItsTime) {
    FrameClock clock;
    std::this_thread::sleep_for(100ms);

    runFrame(clock, 1);

    EXPECT_LT(clock.DeltaTime(), NoTime);
    EXPECT_LT(clock.ElapsedTime(), NoTime);
}

TEST(FrameClock, AHoldsGapIsNotTheNextFramesTime) {
    FrameClock clock;
    runFrame(clock, 1);
    clock.Control().Hold();

    EXPECT_FALSE(clock.Advance());
    std::this_thread::sleep_for(100ms);
    clock.Control().Run();
    runFrame(clock, 2);

    EXPECT_LT(clock.DeltaTime(), NoTime);
    EXPECT_LT(clock.ElapsedTime(), NoTime);
}

TEST(FrameClock, AFreeFrameReadsTheClock) {
    FrameClock clock;
    runFrame(clock, 1);

    std::this_thread::sleep_for(100ms);
    runFrame(clock, 2);

    EXPECT_GE(clock.DeltaTime(), 0.1);
}

TEST(FrameClock, ASteppedFrameTakesTheFixedTickWhateverTheClockSays) {
    FrameClock clock;
    runFrame(clock, 1);
    clock.Control().Hold();
    clock.Control().RunFrames(1);

    std::this_thread::sleep_for(100ms);
    runFrame(clock, 2);

    EXPECT_NEAR(clock.DeltaTime(), TickSeconds, 1e-9);
}

TEST(FrameClock, TheFrameAfterAStepReadsTheClockAgain) {
    FrameClock clock;
    runFrame(clock, 1);
    clock.Control().Hold();
    clock.Control().RunFrames(1);
    runFrame(clock, 2);
    clock.Control().Run();

    std::this_thread::sleep_for(100ms);
    runFrame(clock, 3);

    EXPECT_GE(clock.DeltaTime(), 0.1);
}

TEST(FrameClock, EveryFrameOfACapturingRunTakesTheFixedTick) {
    FrameClock clock(true);
    runFrame(clock, 1);

    std::this_thread::sleep_for(100ms);
    runFrame(clock, 2);

    EXPECT_NEAR(clock.DeltaTime(), TickSeconds, 1e-9);
    EXPECT_NEAR(clock.ElapsedTime(), 2 * TickSeconds, 1e-9);
}

TEST(FrameClock, AQuitReleasesAHold) {
    FrameClock clock;
    runFrame(clock, 1);
    clock.Control().Hold();
    EXPECT_FALSE(clock.Advance());

    clock.RequestQuit();

    EXPECT_TRUE(clock.Advance());
    EXPECT_TRUE(clock.QuitRequested());
}

TEST(FrameClock, AQuitReleasesAHoldThatArrivesAfterIt) {
    FrameClock clock;
    runFrame(clock, 1);

    clock.RequestQuit();
    clock.Control().Hold();

    EXPECT_TRUE(clock.Advance());
}

TEST(FrameClock, AFrameKnowsTheFrameThatBeganLast) {
    FrameClock clock;
    EXPECT_EQ(clock.Frame(), 0u);

    runFrame(clock, 3);

    EXPECT_EQ(clock.Frame(), 3u);
}

TEST(FrameClock, AHoldIsDecidedAtTheFrameThatEndedLast) {
    FrameClock clock;
    runFrame(clock, 1);
    clock.Control().RunUntil(3);

    runFrame(clock, 2);
    runFrame(clock, 3);

    EXPECT_FALSE(clock.Advance());
}
