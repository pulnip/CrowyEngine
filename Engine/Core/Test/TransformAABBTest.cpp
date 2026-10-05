#include <algorithm>
#include <array>
#include <numbers>

#include <gtest/gtest.h>

#include "Geometry/Overlap3D.hpp"
#include "LinearAlgebra.hpp"

using namespace Crowy;

namespace
{
    // the bounds of the box's eight corners carried one by one
    AABB3D bruteForce(const Mat4& m, const AABB3D& box) {
        Vec3 low{1e30f, 1e30f, 1e30f};
        Vec3 high{-1e30f, -1e30f, -1e30f};
        for(u32 corner = 0; corner < 8; ++corner) {
            const Vec3 sign{
                corner & 1 ? 1.0f : -1.0f,
                corner & 2 ? 1.0f : -1.0f,
                corner & 4 ? 1.0f : -1.0f
            };
            const auto local = box.center + Vec3{
                sign.x * box.halfScale.x,
                sign.y * box.halfScale.y,
                sign.z * box.halfScale.z
            };
            const auto world = m * Vec4{local.x, local.y, local.z, 1.0f};
            low = Vec3{std::min(low.x, world.x), std::min(low.y, world.y), std::min(low.z, world.z)};
            high = Vec3{std::max(high.x, world.x), std::max(high.y, world.y), std::max(high.z, world.z)};
        }

        return AABB3D{.center = 0.5f * (low + high), .halfScale = 0.5f * (high - low)};
    }

    void expectNear(const AABB3D& a, const AABB3D& b) {
        constexpr f32 Tolerance = 1e-4f;
        EXPECT_NEAR(a.center.x, b.center.x, Tolerance);
        EXPECT_NEAR(a.center.y, b.center.y, Tolerance);
        EXPECT_NEAR(a.center.z, b.center.z, Tolerance);
        EXPECT_NEAR(a.halfScale.x, b.halfScale.x, Tolerance);
        EXPECT_NEAR(a.halfScale.y, b.halfScale.y, Tolerance);
        EXPECT_NEAR(a.halfScale.z, b.halfScale.z, Tolerance);
    }
}

// a footprint-centered box, not one around the origin, under a yaw, a
// per-axis scale that makes the matrix asymmetric, and an offset
TEST(TransformAABB, MatchesTheCornersUnderYawScaleAndOffset) {
    const AABB3D model{.center = {0.0f, 1.0f, 0.0f}, .halfScale = {1.0f, 1.0f, 0.5f}};
    for(const auto degrees: {0.0f, 30.0f, 90.0f, 135.0f, 270.0f}) {
        const auto m = translateMat({5.0f, 0.0f, 3.0f})
            * rotateYMat(degrees * std::numbers::pi_v<f32> / 180.0f)
            * scaleMat({3.0f, 1.0f, 0.5f});
        expectNear(transformAABB3D(m, model), bruteForce(m, model));
    }
}

TEST(TransformAABB, PerAxisScaleOfTheUnitBox) {
    const AABB3D unit{.center = {0.0f, 0.5f, 0.0f}, .halfScale = {0.5f, 0.5f, 0.5f}};
    const auto m = translateMat({18.65f, 0.15f, 18.65f}) * scaleMat({14.7f, 34.2f, 14.7f});
    const auto box = transformAABB3D(m, unit);

    EXPECT_NEAR(box.center.y, 0.15f + 17.1f, 1e-4f);
    EXPECT_NEAR(box.halfScale.x, 7.35f, 1e-4f);
    EXPECT_NEAR(box.halfScale.y, 17.1f, 1e-4f);
}
