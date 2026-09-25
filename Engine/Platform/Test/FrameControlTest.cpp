#include <gtest/gtest.h>

#include "FrameControl.hpp"

using namespace Crowy;
using Gate = FrameControl::Gate;

TEST(FrameControl, FreeLetsEveryFrameThrough) {
    FrameControl control;

    EXPECT_EQ(control.Advance(0), Gate::Free);
    EXPECT_EQ(control.Advance(7), Gate::Free);
    EXPECT_FALSE(control.IsHeld(7));
    EXPECT_FALSE(control.HoldAt(7).has_value());
}

TEST(FrameControl, HoldHoldsAtTheLastFrame) {
    FrameControl control;
    control.Hold();

    EXPECT_TRUE(control.IsHeld(4));
    EXPECT_EQ(control.HoldAt(4), 4u);
    EXPECT_EQ(control.Advance(4), Gate::Hold);
    EXPECT_EQ(control.Advance(4), Gate::Hold);
}

TEST(FrameControl, RunFramesStepsThatManyThenHolds) {
    FrameControl control;
    control.Hold();
    control.RunFrames(2);

    EXPECT_EQ(control.HoldAt(5), 7u);
    EXPECT_EQ(control.Advance(5), Gate::Step);
    EXPECT_FALSE(control.IsHeld(6));
    EXPECT_EQ(control.Advance(6), Gate::Step);

    EXPECT_TRUE(control.IsHeld(7));
    EXPECT_EQ(control.HoldAt(7), 7u);
    EXPECT_EQ(control.Advance(7), Gate::Hold);
    EXPECT_EQ(control.Advance(7), Gate::Hold);
}

TEST(FrameControl, RunUntilStepsUntilTheFrameHasEnded) {
    FrameControl control;
    control.RunUntil(5);

    for(u64 last = 2; last < 5; ++last) {
        EXPECT_FALSE(control.IsHeld(last));
        EXPECT_EQ(control.HoldAt(last), 5u);
        EXPECT_EQ(control.Advance(last), Gate::Step);
    }

    EXPECT_TRUE(control.IsHeld(5));
    EXPECT_EQ(control.Advance(5), Gate::Hold);
    EXPECT_EQ(control.HoldAt(5), 5u);
}

TEST(FrameControl, RunReleasesAHold) {
    FrameControl control;
    control.Hold();
    EXPECT_EQ(control.Advance(3), Gate::Hold);

    control.Run();

    EXPECT_FALSE(control.IsHeld(3));
    EXPECT_FALSE(control.HoldAt(3).has_value());
    EXPECT_EQ(control.Advance(3), Gate::Free);
}

TEST(FrameControl, ATargetAlreadyReachedHoldsAtOnce) {
    FrameControl control;

    control.RunUntil(3);
    EXPECT_TRUE(control.IsHeld(5));
    EXPECT_EQ(control.HoldAt(5), 5u);
    EXPECT_EQ(control.Advance(5), Gate::Hold);

    control.RunFrames(0);
    EXPECT_TRUE(control.IsHeld(5));
    EXPECT_EQ(control.Advance(5), Gate::Hold);
}
