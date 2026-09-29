#include <gtest/gtest.h>

#include "ShadingModel.hpp"

using namespace Crowy;

// Toon.slang reads custom0 as (shadowThreshold, highlightSize, rimStrength,
// rimWidth) and custom1.rgb as the shade tint; a lane out of place moves the
// wrong term with no error anywhere.
TEST(ShadingModel, ToonLanesPackInTheOrderToonReadsThem) {
    constexpr ToonLanes lanes{
        .shadowThreshold = 0.1f,
        .highlightSize = 0.2f,
        .rimStrength = 0.3f,
        .rimWidth = 0.4f,
        .shadeTint = {0.5f, 0.6f, 0.7f}
    };

    EXPECT_EQ(toonCustom0(lanes), (Vec4{0.1f, 0.2f, 0.3f, 0.4f}));
    EXPECT_EQ(toonCustom1(lanes), (Vec4{0.5f, 0.6f, 0.7f, 0.0f}));

    // the defaults a material switched to Toon starts from: a zero threshold
    // would light shadowed pixels, a black tint blacken the unlit side
    constexpr auto defaults = ToonLanes{};
    EXPECT_GT(toonCustom0(defaults).x, 0.0f);
    EXPECT_GT(toonCustom1(defaults).x + toonCustom1(defaults).y, 0.0f);
}
