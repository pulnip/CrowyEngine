#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"

using namespace Crowy;

namespace
{
    constexpr f32 Pi = std::numbers::pi_v<f32>;

    f32 radians(f32 degrees) {
        return degrees * Pi / 180.0f;
    }

    // q and -q are one rotation
    f32 sameRotation(Vec4 a, Vec4 b) {
        return std::abs(dot(a, b));
    }

    // into (-pi, pi]
    f32 wrapped(f32 angle) {
        return std::remainder(angle, 2.0f * Pi);
    }
}

// within +-90 degrees of pitch the angles come back; past it, and at either
// end, another triple names the same rotation
TEST(Angle, EulerRoundTripsThroughTheQuaternion) {
    for(const auto pitch:
        {-120.0f, -90.0f, -89.0f, -30.0f, 0.0f, 45.0f, 89.0f, 90.0f, 150.0f}) {
        for(const auto yaw: {-170.0f, -45.0f, 0.0f, 30.0f, 135.0f, 180.0f}) {
            for(const auto roll: {-150.0f, 0.0f, 20.0f, 100.0f}) {
                SCOPED_TRACE(
                    testing::Message() << pitch << " " << yaw << " " << roll
                );
                const Vec3 angles{radians(pitch), radians(yaw), radians(roll)};
                const auto q = quatOf(angles);
                const auto back = eulerOf(q);

                EXPECT_NEAR(sameRotation(quatOf(back), q), 1.0f, 1e-5f);
                EXPECT_LE(std::abs(back.x), Pi / 2 + 1e-5f);
                if(std::abs(pitch) < 90.0f) {
                    EXPECT_NEAR(back.x, angles.x, 1e-4f);
                    EXPECT_NEAR(wrapped(back.y - angles.y), 0.0f, 1e-4f);
                    EXPECT_NEAR(wrapped(back.z - angles.z), 0.0f, 1e-4f);
                }
            }
        }
    }
}

TEST(Angle, ValuesSnapToTheNearestStep) {
    EXPECT_FLOAT_EQ(snapTo(22.0f, 15.0f), 15.0f);
    EXPECT_FLOAT_EQ(snapTo(-8.0f, 15.0f), -15.0f);
    EXPECT_FLOAT_EQ(snapTo(7.0f, 15.0f), 0.0f);
    EXPECT_FLOAT_EQ(snapTo(0.9f, 0.25f), 1.0f);
}

TEST(Angle, DegreesWrapIntoOneTurn) {
    EXPECT_FLOAT_EQ(wrapDegrees(370.0f), 10.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(-90.0f), 270.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(360.0f), 0.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(-360.0f), 0.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(270.0f), 270.0f);
    const auto hair = wrapDegrees(-1e-6f);
    EXPECT_GE(hair, 0.0f);
    EXPECT_LT(hair, 360.0f);
}
