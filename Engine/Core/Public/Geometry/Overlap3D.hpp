#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct AABB3D{
        Vec3 center = zeros();
        Vec3 halfScale = 0.5f * ones();
    };

    // the box around `box` carried by an affine matrix (Arvo): the centre
    // moves with the matrix, each half extent sums the |columns| it spans
    inline constexpr AABB3D transformAABB3D(const Mat4& m, const AABB3D& box) noexcept{
        const auto reach = [](const Vec4& column, f32 half) noexcept{
            const auto magnitude = [](f32 v) noexcept{ return v < 0.0f ? -v : v; };

            return half * Vec3{magnitude(column.x), magnitude(column.y), magnitude(column.z)};
        };
        const auto center = m * Vec4{box.center.x, box.center.y, box.center.z, 1.0f};

        return AABB3D{
            .center = Vec3{center.x, center.y, center.z},
            .halfScale = reach(m[0], box.halfScale.x)
                + reach(m[1], box.halfScale.y)
                + reach(m[2], box.halfScale.z)
        };
    }
}
