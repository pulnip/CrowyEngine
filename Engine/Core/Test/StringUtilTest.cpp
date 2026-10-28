#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "StringUtil.hpp"

using namespace Crowy;

namespace
{
    struct Named {
        StrView name;
    };
}

TEST(Join, LeadingEmptyToken) {
    static_assert(join(std::array<Str, 2>{"", "a"}, ",") == ",a");
}

TEST(Join, ProjectsEachToken) {
    static_assert(
        join(std::array{Named{"a"}, Named{"b"}}, ", ", &Named::name) == "a, b"
    );
}

TEST(ConsumePrefix, StripsOnlyAPrefixThatIsThere) {
    static_assert([] {
        StrView text = "engine:a.png";
        return consumePrefix(text, "engine:") && text == "a.png";
    }());
    static_assert([] {
        StrView text = "a.png";
        return !consumePrefix(text, "engine:") && text == "a.png";
    }());
}

TEST(ReplaceAll, ReplacesEveryOccurrenceOnce) {
    static_assert(replaceAll("f-{n}{n}.bmp", "{n}", "60") == "f-6060.bmp");
    static_assert(replaceAll("{n}", "{n}", "{n}{n}") == "{n}{n}");
    static_assert(replaceAll("plain", "{n}", "60") == "plain");
    static_assert(replaceAll("plain", "", "60") == "plain");
}

TEST(ParseNumber, TakesOnlyTheWholeText) {
    static_assert(parseNumber<u32>("42") == 42u);
    static_assert(parseNumber<u32>("42x") == std::nullopt);
    static_assert(parseNumber<u32>("") == std::nullopt);
    static_assert(parseNumber<u8>("256") == std::nullopt);
    static_assert(parseNumber<i32>("-3") == -3);
    EXPECT_EQ(parseNumber<f32>("0.5"), 0.5f);
    EXPECT_EQ(parseNumber<f32>("0.5 "), std::nullopt);
}

TEST(Case, FoldsAsciiLettersOnly) {
    static_assert(lower("Content-Length\xC3\x89") == "content-length\xC3\x89");
    static_assert(equalsIgnoringCase("Content-Length", "content-LENGTH"));
    static_assert(!equalsIgnoringCase("Content", "Contents"));
    static_assert(containsIgnoringCase("PhysicsPlayground", "PLAY"));
    static_assert(containsIgnoringCase("anything", ""));
    static_assert(!containsIgnoringCase("Box", "boxes"));
}
