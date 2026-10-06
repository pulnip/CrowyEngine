#include <gtest/gtest.h>

#include "MintFrame.hpp"

using namespace Crowy;

namespace
{
    void expectNear(Vec3 actual, Vec3 expected, f32 tolerance = 1e-5f) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    void expectNear(DVec3 actual, DVec3 expected, f64 tolerance = 1e-9) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }
}

TEST(MintFrame, BoundaryPermutesAxesAndScalesToMetres) {
    // Mint forward is Crowy +Z, Mint up is Crowy +Y, Mint right Crowy +X
    expectNear(toCrowyPoint({100.0, 0.0, 0.0}), {0.0f, 0.0f, 1.0f});
    expectNear(toCrowyPoint({0.0, 100.0, 0.0}), {1.0f, 0.0f, 0.0f});
    expectNear(toCrowyPoint({0.0, 0.0, 100.0}), {0.0f, 1.0f, 0.0f});

    const auto m = mintToCrowy();
    const auto p = m * Vec4{100.0f, 200.0f, 300.0f, 1.0f};
    expectNear(static_cast<Vec3>(p), toCrowyPoint({100.0, 200.0, 300.0}));

    // a cyclic permutation keeps handedness
    const auto x = static_cast<Vec3>(m[0]);
    const auto y = static_cast<Vec3>(m[1]);
    const auto z = static_cast<Vec3>(m[2]);
    EXPECT_GT(dot(x, cross(y, z)), 0.0f);
}

TEST(MintFrame, BoundaryRoundTrips) {
    const DVec3 p{-1300.0, -1500.0, 900.0};
    expectNear(fromCrowyPoint(toCrowyPoint(p)), p, 1e-3);
    const DVec3 v{0.25, -0.5, 0.75};
    expectNear(fromCrowyVector(toCrowyVector(v)), v, 1e-7);
}

TEST(MintFrame, YawTurnsForwardToRight) {
    const auto yaw90 = DQuat::FromRotator(0.0, 90.0, 0.0);
    expectNear(yaw90.Rotate({1.0, 0.0, 0.0}), {0.0, 1.0, 0.0});
    expectNear(yaw90.Unrotate({0.0, 1.0, 0.0}), {1.0, 0.0, 0.0});
}

TEST(MintFrame, TransformKeepsScaleOutOfNoScaleInverses) {
    const MintTransform t{
        .rotation = DQuat::FromRotator(0.0, 90.0, 0.0),
        .translation = {10.0, 20.0, 30.0},
        .scale = {2.0, 3.0, 4.0}
    };
    // scale first, then the yaw, then the move
    expectNear(t.TransformPosition({1.0, 0.0, 0.0}), {10.0, 22.0, 30.0});
    // the scaled-local frame: rotation and translation removed, scale kept
    expectNear(
        t.InverseTransformPositionNoScale({10.0, 22.0, 30.0}),
        {2.0, 0.0, 0.0}
    );
    expectNear(
        t.InverseTransformVectorNoScale({0.0, 1.0, 0.0}),
        {1.0, 0.0, 0.0}
    );

    const auto m = t.ToMat4();
    const auto p = m * Vec4{1.0f, 0.0f, 0.0f, 1.0f};
    expectNear(static_cast<Vec3>(p), {10.0f, 22.0f, 30.0f});
}

TEST(MintFrame, SafeNormalFollowsUnreal) {
    expectNear(getSafeNormal({3.0, 0.0, 4.0}), {0.6, 0.0, 0.8});
    expectNear(getSafeNormal({1e-5, 0.0, 0.0}), {0.0, 0.0, 0.0});
    expectNear(
        rotateAngleAxis({1.0, 0.0, 0.0}, 90.0, {0.0, 0.0, 1.0}),
        {0.0, 1.0, 0.0}
    );
}

TEST(MintFrame, RectClipStaysNonNegative) {
    IntRect r{{-5, 10}, {20, 40}};
    r.Clip({{0, 0}, {16, 16}});
    EXPECT_EQ(r, (IntRect{{0, 10}, {16, 16}}));
    EXPECT_EQ(r.Area(), 16 * 6);

    IntRect outside{{30, 30}, {40, 40}};
    outside.Clip({{0, 0}, {16, 16}});
    EXPECT_TRUE(outside.IsEmpty());
}
