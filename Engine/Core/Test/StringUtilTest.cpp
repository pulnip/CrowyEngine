#include <array>

#include <gtest/gtest.h>

#include "StringUtil.hpp"

using namespace Crowy;

TEST(Join, LeadingEmptyToken) {
    static_assert(join(std::array<Str, 2>{"", "a"}, ",") == ",a");
}
