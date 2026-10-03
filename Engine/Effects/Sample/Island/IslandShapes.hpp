#pragma once

#include <cmath>
#include <numbers>

#include "IslandScene.h"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

// The CPU twin of IslandShapes.slang, equal to the GPU's within a check's
// tolerance: the sand's crown and the tipi's pyramid.
namespace Crowy
{
    inline constexpr Vec3 IslandRadii{ISLAND_RADII};
    inline constexpr f32 TipiFacetAngle = 2.0f * std::numbers::pi_v<f32> /
                                          static_cast<f32>(ISLAND_TIPI_FACETS);

    // the sand's height, or the ellipsoid's center where it does not reach
    inline f32 islandCrown(Vec2 xz) {
        const auto qx = xz.x / IslandRadii.x;
        const auto qz = xz.y / IslandRadii.z;
        const auto inside = 1.0f - (qx * qx + qz * qz);

        return inside > 0.0f
                   ? ISLAND_CENTER_Y + IslandRadii.y * std::sqrt(inside)
                   : ISLAND_CENTER_Y;
    }

    // facet k's outward direction across the ground
    inline Vec2 tipiFacetNormal(u32 k) {
        const auto azimuth =
            ISLAND_TIPI_FIRST_POLE +
            (static_cast<f32>(k) + 0.5f) * TipiFacetAngle;

        return Vec2{std::sin(azimuth), std::cos(azimuth)};
    }

    // how far out xz lies across the facets, and across which one
    inline f32 tipiReach(Vec2 xz, u32& facet) {
        auto reach = dot(xz, tipiFacetNormal(0));
        facet = 0;
        for(u32 k = 1; k < ISLAND_TIPI_FACETS; ++k) {
            const auto across = dot(xz, tipiFacetNormal(k));
            if(across > reach) {
                reach = across;
                facet = k;
            }
        }

        return reach;
    }

    // the pyramid the canvas lies on, up to the poles' crossing
    inline f32 tipiRoof(Vec2 xz) {
        u32 facet = 0;
        const auto reach = tipiReach(xz, facet);
        const auto base = ISLAND_TIPI_RADIUS * std::cos(0.5f * TipiFacetAngle);

        return ISLAND_TIPI_APEX_Y -
               (ISLAND_TIPI_APEX_Y - ISLAND_TIPI_BASE_Y) * reach / base;
    }
}
