#include <gtest/gtest.h>

#include "FrameHistory.hpp"

using namespace Crowy;

namespace
{
    using History = FrameHistory<int, 4>;

    // frames 1..count, each holding ten times its number
    History pushed(u64 count) {
        History history;
        for(u64 frame = 1; frame <= count; ++frame)
            history.Push(frame, static_cast<int>(frame * 10));
        return history;
    }
}

TEST(FrameHistory, FindsWhatWasPushed) {
    const auto history = pushed(3);

    ASSERT_NE(history.Find(2), nullptr);
    EXPECT_EQ(*history.Find(2), 20);
    EXPECT_EQ(*history.Find(3), 30);
    EXPECT_EQ(history.Find(4), nullptr);
}

TEST(FrameHistory, ForgetsWhatWasOverwritten) {
    const auto history = pushed(6);

    EXPECT_EQ(history.Find(1), nullptr);
    EXPECT_EQ(history.Find(2), nullptr);

    for(u64 frame = 3; frame <= 6; ++frame) {
        ASSERT_NE(history.Find(frame), nullptr) << frame;
        EXPECT_EQ(*history.Find(frame), static_cast<int>(frame * 10));
    }
}

TEST(FrameHistory, NewestAndOldestFollowTheWindow) {
    EXPECT_EQ(History{}.Newest(), 0u);
    EXPECT_EQ(History{}.Oldest(), 0u);

    EXPECT_EQ(pushed(1).Oldest(), 1u);
    EXPECT_EQ(pushed(History::Depth).Oldest(), 1u);

    const auto history = pushed(History::Depth + 2);

    EXPECT_EQ(history.Newest(), History::Depth + 2);
    EXPECT_EQ(history.Oldest(), 3u);
}

TEST(FrameHistory, FrameZeroNeverMatches) {
    EXPECT_EQ(History{}.Find(0), nullptr);
    // slot 0 holds frame 4 here
    EXPECT_EQ(pushed(4).Find(0), nullptr);
}
