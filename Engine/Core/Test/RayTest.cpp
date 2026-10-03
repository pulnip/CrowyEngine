#include <limits>

#include <gtest/gtest.h>

#include "Geometry/Ray3D.hpp"

using namespace Crowy;

namespace
{
    constexpr AABB3D UnitBox{.center = {0.0f, 0.0f, 5.0f}, .halfScale = {1.0f, 1.0f, 1.0f}};

    // a square in z = 0 facing -Z (toward a viewer at -Z), wound as MakePlane
    // winds a quad facing -Z with u along +X
    constexpr Vec3 A{-1.0f, 1.0f, 0.0f};
    constexpr Vec3 B{1.0f, 1.0f, 0.0f};
    constexpr Vec3 C{1.0f, -1.0f, 0.0f};
}

TEST(Ray, EntersABoxInFront) {
    const auto t = intersectRayAABB3D(Ray3D{.origin = {0.0f, 0.0f, 0.0f}}, UnitBox);
    ASSERT_TRUE(t.has_value());
    EXPECT_FLOAT_EQ(*t, 4.0f);
}

TEST(Ray, InsideTheBoxIsZero) {
    const auto t = intersectRayAABB3D(Ray3D{.origin = {0.0f, 0.0f, 5.0f}}, UnitBox);
    ASSERT_TRUE(t.has_value());
    EXPECT_FLOAT_EQ(*t, 0.0f);
}

TEST(Ray, MissesABoxBehindOrBeside) {
    EXPECT_FALSE(intersectRayAABB3D(Ray3D{.origin = {0.0f, 0.0f, 10.0f}}, UnitBox));
    EXPECT_FALSE(intersectRayAABB3D(Ray3D{.origin = {2.5f, 0.0f, 0.0f}}, UnitBox));
    // parallel to a face and outside its slab
    EXPECT_FALSE(intersectRayAABB3D(Ray3D{.origin = {0.0f, 3.0f, 0.0f}, .direction = {0.0f, 0.0f, 1.0f}}, UnitBox));
}

TEST(Ray, HitsAFlatBox) {
    const AABB3D quad{.center = {0.0f, 0.0f, 3.0f}, .halfScale = {1.0f, 1.0f, 0.0f}};
    const auto t = intersectRayAABB3D(Ray3D{.origin = {0.5f, 0.5f, 0.0f}}, quad);
    ASSERT_TRUE(t.has_value());
    EXPECT_FLOAT_EQ(*t, 3.0f);
}

TEST(Ray, TriangleHitFromTheFrontOnly) {
    const auto front = intersectRayTriangle(Ray3D{.origin = {0.5f, 0.2f, -2.0f}, .direction = {0.0f, 0.0f, 1.0f}}, A, B, C);
    ASSERT_TRUE(front.has_value());
    EXPECT_FLOAT_EQ(*front, 2.0f);

    const Ray3D behind{.origin = {0.5f, 0.2f, 2.0f}, .direction = {0.0f, 0.0f, -1.0f}};
    EXPECT_FALSE(intersectRayTriangle(behind, A, B, C));
    EXPECT_TRUE(intersectRayTriangle(behind, A, B, C, false));
}

TEST(Ray, TriangleMissedJustOutside) {
    // the triangle A B C covers the half above the diagonal from A to C
    EXPECT_FALSE(intersectRayTriangle(Ray3D{.origin = {-0.5f, -0.6f, -2.0f}}, A, B, C));
    EXPECT_FALSE(intersectRayTriangle(Ray3D{.origin = {1.01f, 0.0f, -2.0f}}, A, B, C));
}

// a ray from a zero-size viewport or a zero-scale row hits nothing
TEST(Ray, ANaNRayHitsNothing) {
    constexpr auto NaN = std::numeric_limits<f32>::quiet_NaN();
    const Ray3D direction{.origin = {0.0f, 0.0f, 0.0f}, .direction = {NaN, NaN, NaN}};
    const Ray3D origin{.origin = {NaN, 0.0f, NaN}, .direction = {0.0f, 0.0f, 1.0f}};

    EXPECT_FALSE(intersectRayAABB3D(direction, UnitBox));
    EXPECT_FALSE(intersectRayAABB3D(origin, UnitBox));
    EXPECT_FALSE(intersectRayTriangle(direction, A, B, C));
    EXPECT_FALSE(intersectRayTriangle(origin, A, B, C));
}
