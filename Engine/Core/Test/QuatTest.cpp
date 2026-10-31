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
