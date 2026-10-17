#include <expected>
#include <memory>
#include <stdexcept>

#include <gtest/gtest.h>

#include "Expected.hpp"

using namespace Crowy;

TEST(ValueOrThrow, AValueComesOutMoved) {
    std::expected<RAII<i32>, Str> result = std::make_unique<i32>(7);

    const auto value = valueOrThrow(std::move(result));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 7);
}

TEST(ValueOrThrow, AnErrorThrowsItsReason) {
    try {
        valueOrThrow(std::expected<i32, Str>(std::unexpect, "out of space"));
        FAIL() << "no throw";
    }
    catch(const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "out of space");
    }
}
