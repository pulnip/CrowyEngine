#pragma once

#include <array>
#include "GeoUtil.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct Ball3D{
        Vec3 center = zeros();
        f32 radius = 1.0f;
    };
    using Sphere = Ball3D;

    inline constexpr auto OverlapBall2D(const Ball3D b0, const Ball3D b1) noexcept{
        const auto diff = b1.center - b0.center;
        const auto r2 = b0.radius + b1.radius;

        return normSquared(diff) <= r2*r2;
    }

    struct AABB3D{
        Vec3 center = zeros();
        Vec3 halfScale = 0.5f * ones();
    };
    inline constexpr auto makeAABB3D(Transform t) noexcept{
        return AABB3D{
            .center = t.position,
            .halfScale = t.scale / 2
        };
    }

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

    inline constexpr auto OverlapAABB3D(
        const AABB3D b0, const AABB3D b1,
        const f32 epsilon = 0.0f
    ) noexcept{
        const auto overlap_x = overlap(
            b0.center.x - b0.halfScale.x, b0.center.x + b0.halfScale.x,
            b1.center.x - b1.halfScale.x, b1.center.x + b1.halfScale.x,
            epsilon
        );
        const auto overlap_y = overlap(
            b0.center.y - b0.halfScale.y, b0.center.y + b0.halfScale.y,
            b1.center.y - b1.halfScale.y, b1.center.y + b1.halfScale.y,
            epsilon
        );
        const auto overlap_z = overlap(
            b0.center.z - b0.halfScale.z, b0.center.z + b0.halfScale.z,
            b1.center.z - b1.halfScale.z, b1.center.z + b1.halfScale.z,
            epsilon
        );

        return overlap_x && overlap_y && overlap_z;
    }

    struct OBB3D{
        Vec3 center{.x = 0.0f, .y = 0.0f};
        std::array<Vec3, 3> halfAxes = {
            0.5f * unitX(),
            0.5f * unitY(),
            0.5f * unitZ()
        };
    };
    inline constexpr auto makeOBB3D(Transform t) noexcept{
        return OBB3D{
            .center = t.position,
            .halfAxes = {
                (0.5f * t.scale.x) * right(t.rotation),
                (0.5f * t.scale.y) * up(t.rotation),
                (0.5f * t.scale.z) * forward(t.rotation)
            }
        };
    }

    namespace detail{
        inline constexpr auto project(const Vec3 axis, const OBB3D b){
            return Projected{
                .center = dot(axis, b.center),
                .radius =
                    abs(dot(axis, b.halfAxes[0])) +
                    abs(dot(axis, b.halfAxes[1])) +
                    abs(dot(axis, b.halfAxes[2]))
            };
        }
    }

    inline constexpr bool OverlapOBB3D(
        const OBB3D b0, const OBB3D b1,
        const f32 epsilon = 0.0f
    ){
        using detail::project;

        const std::array axes = {
            // b0
            cross(b0.halfAxes[1], b0.halfAxes[2]),
            cross(b0.halfAxes[2], b0.halfAxes[0]),
            cross(b0.halfAxes[0], b0.halfAxes[1]),
            // b1
            cross(b1.halfAxes[1], b1.halfAxes[2]),
            cross(b1.halfAxes[2], b1.halfAxes[0]),
            cross(b1.halfAxes[0], b1.halfAxes[1]),
            // b0-b1
            cross(b0.halfAxes[0], b1.halfAxes[0]),
            cross(b0.halfAxes[0], b1.halfAxes[1]),
            cross(b0.halfAxes[0], b1.halfAxes[2]),
            cross(b0.halfAxes[1], b1.halfAxes[0]),
            cross(b0.halfAxes[1], b1.halfAxes[1]),
            cross(b0.halfAxes[1], b1.halfAxes[2]),
            cross(b0.halfAxes[2], b1.halfAxes[0]),
            cross(b0.halfAxes[2], b1.halfAxes[1]),
            cross(b0.halfAxes[2], b1.halfAxes[2])
        };

        for(const auto axis: axes){
            auto p0 = project(axis, b0);
            auto p1 = project(axis, b1);

            if(!Overlap(p0, p1, epsilon))
                return false;
        }

        return true;
    }
}
