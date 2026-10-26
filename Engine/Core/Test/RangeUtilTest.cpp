#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Primitives.hpp"
#include "RangeUtil.hpp"

using namespace Crowy;

namespace
{
    struct Named {
        StrView name;
        u32 order = 0;
    };
}

TEST(Stray, KeepsTheOrderOfKeys) {
    static_assert(
        stray(std::array<Str, 3>{"y", "a", "x"}, std::array<StrView, 1>{"a"}) ==
        std::vector<Str>{"y", "x"}
    );
}

TEST(FirstRepeat, PointsAtTheSecondOfTheEarliestPair) {
    constexpr std::array<Named, 4> Rows{
        Named{"a", 0},
        Named{"b", 1},
        Named{"a", 2},
        Named{"b", 3},
    };
    static_assert(firstRepeat(Rows, &Named::name)->order == 2);

    constexpr std::array<Named, 2> Unique{Named{"a", 0}, Named{"b", 1}};
    static_assert(firstRepeat(Unique, &Named::name) == Unique.end());
}
