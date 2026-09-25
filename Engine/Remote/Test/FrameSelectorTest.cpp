#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "FrameSelector.hpp"
#include "JsonLoader.hpp"

using namespace Crowy;

namespace
{
    FrameSelector parse(StrView args) {
        return parseFrameSelector(parseJsonString(args));
    }
}

TEST(FrameSelector, OneFrame) {
    const auto selector = parse(R"({"frame": 7})");

    EXPECT_TRUE(selector.error.empty()) << selector.error;
    EXPECT_EQ(selector.frames, (std::vector<u64>{7}));
}

TEST(FrameSelector, AListKeepsRequestOrder) {
    const auto selector = parse(R"({"frames": [3, 1, 2]})");

    EXPECT_TRUE(selector.error.empty()) << selector.error;
    EXPECT_EQ(selector.frames, (std::vector<u64>{3, 1, 2}));
}

TEST(FrameSelector, ARangeIsInclusive) {
    const auto selector = parse(R"({"from": 4, "to": 6})");

    EXPECT_TRUE(selector.error.empty()) << selector.error;
    EXPECT_EQ(selector.frames, (std::vector<u64>{4, 5, 6}));
    EXPECT_EQ(parse(R"({"from": 5, "to": 5})").frames, (std::vector<u64>{5}));
}

TEST(FrameSelector, NoSelectorIsEmptyAndNoError) {
    const auto selector = parse(R"({"path": "unrelated"})");

    EXPECT_TRUE(selector.frames.empty());
    EXPECT_TRUE(selector.error.empty()) << selector.error;
}

TEST(FrameSelector, EachMalformedFormNamesItsFault) {
    const std::vector<std::pair<StrView, StrView>> cases{
        {R"({"frame": 0})", "frame must be a positive integer"},
        {R"({"frame": -2})", "frame must be a positive integer"},
        {R"({"frame": 1.5})", "frame must be a positive integer"},
        {R"({"frame": "3"})", "frame must be a positive integer"},
        {R"({"frames": []})", "frames must be a non-empty array of positive integers"},
        {R"({"frames": 3})", "frames must be a non-empty array of positive integers"},
        {R"({"frames": [1, 0]})", "frames must be a non-empty array of positive integers"},
        {R"({"frames": [2, 5, 2]})", "frames must not repeat"},
        {R"({"from": 3})", "from and to must both be positive integers"},
        {R"({"from": 3, "to": 1})", "from must not exceed to"},
        {R"({"frame": 1, "frames": [2]})", "use one of frame, frames, or from/to"},
        {R"({"frame": 1, "to": 2})", "use one of frame, frames, or from/to"},
    };

    for(const auto& [args, error]: cases) {
        const auto selector = parse(args);

        EXPECT_EQ(selector.error, error) << args;
        EXPECT_TRUE(selector.frames.empty()) << args;
    }
}

TEST(FrameSelector, NamesAtMostSixtyFourFrames) {
    EXPECT_EQ(parse(R"({"from": 1, "to": 64})").frames.size(), 64u);
    EXPECT_EQ(
        parse(R"({"from": 1, "to": 65})").error,
        "a selector names at most 64 frames"
    );

    Str list = R"({"frames": [1)";
    for(int frame = 2; frame <= 65; ++frame)
        list += ", " + std::to_string(frame);
    list += "]}";

    EXPECT_EQ(parse(list).error, "a selector names at most 64 frames");
}
