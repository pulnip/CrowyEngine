#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Primitives.hpp"
#include "RangeUtil.hpp"

using namespace Crowy;

TEST(Stray, KeepsTheOrderOfKeys) {
    static_assert(
        stray(std::array<Str, 3>{"y", "a", "x"}, std::array<StrView, 1>{"a"}) ==
        std::vector<Str>{"y", "x"}
    );
}
