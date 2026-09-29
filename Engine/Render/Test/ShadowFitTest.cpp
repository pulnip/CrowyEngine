#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"
#include "ShadowFit.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 MapSize = 2048;
    // clip space, give or take a float's rounding
    constexpr f32 Slack = 1e-5f;

    // Playground's room: walls 0.2 m thick around a 20 m floor, 4 m high
    constexpr AABB3D Room{
        .center = {0.0f, 2.0f, 0.0f},
        .halfScale = {10.2f, 2.0f, 10.2f}
    };

    const auto Sun = -normalize(Vec3{0.25f, 0.866f, -0.433f});
    const std::array Travels{
        Sun,
        -unitY(),
        normalize(Vec3{1.0f, -0.05f, 0.3f})
    };

    Vec4 Clip(const DirectionalShadowFit& fit, Vec3 world) {
        return fit.worldToShadow * toVec4(world, 1.0f);
    }

    bool Finite(const Mat4& m) {
        for(const auto& c: m) {
            if(!std::isfinite(c.x) || !std::isfinite(c.y) ||
               !std::isfinite(c.z) || !std::isfinite(c.w))
                return false;
        }

        return true;
    }
}

TEST(ShadowFit, EverySceneCornerIsInsideClipSpace) {
    for(const auto travel: Travels) {
        const auto fit = fitDirectionalShadow(travel, Room, MapSize);

        for(u32 corner = 0; corner < 8; ++corner) {
            const Vec3 side{
                (corner & 1u) != 0 ? 1.0f : -1.0f,
                (corner & 2u) != 0 ? 1.0f : -1.0f,
                (corner & 4u) != 0 ? 1.0f : -1.0f
            };
            const auto clip = Clip(fit, Room.center + side * Room.halfScale);

            EXPECT_EQ(clip.w, 1.0f);
            EXPECT_LE(std::abs(clip.x), 1.0f + Slack) << corner;
            EXPECT_LE(std::abs(clip.y), 1.0f + Slack) << corner;
            // the margin keeps every corner off both planes
            EXPECT_GT(clip.z, 0.0f) << corner;
            EXPECT_LT(clip.z, 1.0f) << corner;
        }
    }

    // the numbers the task file derived for Playground's sun
    const auto fit = fitDirectionalShadow(Sun, Room, MapSize);
    EXPECT_NEAR(fit.texelSize * MapSize, 27.87f, 0.01f);
    EXPECT_NEAR(fit.texelSize * 100.0f, 1.36f, 0.01f);
}

TEST(ShadowFit, TravelMapsToPositiveZ) {
    for(const auto travel: Travels) {
        const auto fit = fitDirectionalShadow(travel, Room, MapSize);
        const auto near = Clip(fit, Room.center);
        const auto far = Clip(fit, Room.center + travel);

        EXPECT_GT(far.z, near.z);
        EXPECT_NEAR(far.x, near.x, Slack);
        EXPECT_NEAR(far.y, near.y, Slack);
    }
}

TEST(ShadowFit, AStraightDownSunGivesNoNaN) {
    for(const auto travel: {-unitY(), Vec3{0.0f, -2.0f, 0.0f}, unitY()}) {
        const auto fit = fitDirectionalShadow(travel, Room, MapSize);

        EXPECT_TRUE(Finite(fit.worldToShadow));
        EXPECT_TRUE(std::isfinite(fit.texelSize));
        EXPECT_GT(fit.texelSize, 0.0f);
    }
}

// a texel's step along the light's right or up is one texel in clip space,
// whichever of the two the scene is longer along
TEST(ShadowFit, TexelsAreSquare) {
    constexpr AABB3D Long{.center = zeros(), .halfScale = {10.0f, 1.0f, 1.0f}};
    constexpr f32 Texel = 2.0f / MapSize;

    for(const auto travel: Travels) {
        const auto fit = fitDirectionalShadow(travel, Long, MapSize);
        // the fit's own basis, as lookAt builds it
        const auto forward = normalize(travel);
        const auto up = std::abs(forward.y) > 0.99f ? unitZ() : unitY();
        const auto right = normalize(cross(up, forward));
        const auto lightUp = cross(forward, right);

        const auto origin = Clip(fit, zeros());
        const auto alongRight = Clip(fit, fit.texelSize * right);
        const auto alongUp = Clip(fit, fit.texelSize * lightUp);

        EXPECT_NEAR(alongRight.x - origin.x, Texel, 1e-6f);
        EXPECT_NEAR(alongRight.y - origin.y, 0.0f, 1e-6f);
        EXPECT_NEAR(alongUp.x - origin.x, 0.0f, 1e-6f);
        EXPECT_NEAR(alongUp.y - origin.y, Texel, 1e-6f);
    }
}

TEST(ShadowFit, APointSizedSceneKeepsACentimetre) {
    constexpr AABB3D Point{.center = {1.0f, 2.0f, 3.0f}, .halfScale = zeros()};

    const auto fit = fitDirectionalShadow(Sun, Point, MapSize);

    EXPECT_TRUE(Finite(fit.worldToShadow));
    EXPECT_FLOAT_EQ(fit.texelSize, 0.01f / MapSize);
    const auto clip = Clip(fit, Point.center);
    EXPECT_NEAR(clip.x, 0.0f, Slack);
    EXPECT_NEAR(clip.y, 0.0f, Slack);
    EXPECT_GT(clip.z, 0.0f);
    EXPECT_LT(clip.z, 1.0f);
}
