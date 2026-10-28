#pragma once

#include <gtest/gtest.h>

#include "Primitives.hpp"

namespace Crowy
{
    inline void expectNear(Vec3 actual, Vec3 expected, f32 tolerance = 1e-4f) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }
}
