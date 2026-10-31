#include <type_traits>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"

using namespace Crowy;

TEST(Quat, ConvertsToAndFromVec4WithoutACast) {
    const Vec4 plain{0.1f, 0.2f, 0.3f, 0.9f};
    const Quat rotation = plain;
    const Vec4 back = rotation;
    EXPECT_EQ(back, plain);

    const Transform transform{.rotation = plain};
    const Vec4 stored = transform.rotation;
    EXPECT_EQ(stored, plain);
    EXPECT_EQ(rotation, plain);
}

TEST(Quat, DefaultsToTheIdentity) {
    EXPECT_EQ(Quat{}, unitQuat());
    EXPECT_EQ(Transform{}.rotation, unitQuat());
}

TEST(Quat, RotationMathAcceptsAQuat) {
    const Quat turn = rotateY(radiansOf(90.0f));
    const auto rotated = rotate(Vec3{0.0f, 0.0f, 1.0f}, turn);
    EXPECT_NEAR(rotated.x, 1.0f, 1e-6f);
    EXPECT_NEAR(rotated.z, 0.0f, 1e-6f);
    EXPECT_EQ(conjugate(turn), Vec4(-turn.x, -turn.y, -turn.z, turn.w));
}

TEST(Quat, IsNotAVec4ToTheTypeSystem) {
    static_assert(!std::is_same_v<Quat, Vec4>);
    static_assert(sizeof(Quat) == sizeof(Vec4));
}

TEST(Quat, ALookRotationPointsForwardAlongItsDirection) {
    const Vec3 directions[] = {
        normalize(Vec3{0.3531f, -0.788f, 0.5043f}),
        normalize(Vec3{-1.0f, 0.5f, -2.0f}),
        normalize(Vec3{1.0f, 0.0f, 0.0f}),
        Vec3{0.0f, 0.0f, -1.0f},
        Vec3{0.0f, -1.0f, 0.0f},
        Vec3{0.0f, 1.0f, 0.0f},
    };

    for(const auto& direction: directions) {
        const auto ahead = forward(lookRotation(direction));
        EXPECT_NEAR(ahead.x, direction.x, 1e-5f);
        EXPECT_NEAR(ahead.y, direction.y, 1e-5f);
        EXPECT_NEAR(ahead.z, direction.z, 1e-5f);
    }
}
