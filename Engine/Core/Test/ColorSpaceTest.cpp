#include <cmath>

#include <gtest/gtest.h>

#include "ColorSpace.hpp"

using namespace Crowy;

// every byte value against SRGB.slang's formula: lerp(lo, hi, step(0.04045, x))
TEST(ColorSpace, MatchesTheShaderCurve) {
    for(u32 byte = 0; byte < 256; ++byte) {
        const auto x = static_cast<f32>(byte) / 255.0f;
        const auto expected = x >= 0.04045f
            ? std::pow((x + 0.055f) / 1.055f, 2.4f)
            : x / 12.92f;
        EXPECT_NEAR(srgbToLinear(x), expected, 1e-6f) << byte;
    }
    EXPECT_FLOAT_EQ(srgbToLinear(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(srgbToLinear(1.0f), 1.0f);
    EXPECT_NEAR(srgbToLinear(0.5f), 0.214041f, 1e-5f);
}

TEST(ColorSpace, AppliesPerChannel) {
    // #F2B705, the axis probe's core
    const auto color = srgbToLinear(Vec3{242.0f / 255.0f, 183.0f / 255.0f, 5.0f / 255.0f});
    EXPECT_NEAR(color.x, 0.88792f, 1e-5f);
    EXPECT_NEAR(color.y, 0.47353f, 1e-5f);
    EXPECT_NEAR(color.z, 0.0015176f, 1e-6f);
}
