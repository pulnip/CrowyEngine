#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct AABB3D {
        Vec3 center = zeros();
        Vec3 halfScale = 0.5f * ones();
    };

    // bits 0, 1 and 2 pick the +x, +y and +z sides
    inline constexpr Vec3 cornerOf(const AABB3D& box, u32 corner) noexcept {
        const Vec3 side{
            (corner & 1u) != 0 ? 1.0f : -1.0f,
            (corner & 2u) != 0 ? 1.0f : -1.0f,
            (corner & 4u) != 0 ? 1.0f : -1.0f
        };

        return box.center + side * box.halfScale;
    }

    // the box around `box` carried by an affine matrix (Arvo): the centre
    // moves with the matrix, each half extent sums the |columns| it spans
    inline constexpr AABB3D transformAABB3D(
        const Mat4& m,
        const AABB3D& box
    ) noexcept {
        const auto reach = [](const Vec4& column, f32 half) noexcept {
            const auto magnitude = [](f32 v) noexcept {
                return v < 0.0f ? -v : v;
            };

            return half * Vec3{
                              magnitude(column.x),
                              magnitude(column.y),
                              magnitude(column.z)
                          };
        };

        return AABB3D{
            .center = transformPoint(m, box.center),
            .halfScale = reach(m[0], box.halfScale.x) +
                         reach(m[1], box.halfScale.y) +
                         reach(m[2], box.halfScale.z)
        };
    }
}
