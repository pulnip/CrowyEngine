#pragma once

#include <algorithm>
#include <optional>

#include "Geometry/Overlap3D.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct Ray3D;

    // where the ray enters the box, 0 from inside; nullopt when it misses
    // or the box is behind it
    inline constexpr std::optional<f32> intersectRayAABB3D(const Ray3D& ray, const AABB3D& box) noexcept;

    // t of the hit, nullopt for a miss; `frontOnly` refuses a triangle the
    // ray meets from behind, a front being clockwise as MeshData's are
    inline constexpr std::optional<f32> intersectRayTriangle(
        const Ray3D& ray,
        Vec3 a,
        Vec3 b,
        Vec3 c,
        bool frontOnly = true
    ) noexcept;

    // origin + t * direction; direction need not be unit, and t scales with it
    struct Ray3D {
        Vec3 origin{};
        Vec3 direction{0.0f, 0.0f, 1.0f};
    };

    inline constexpr std::optional<f32> intersectRayAABB3D(const Ray3D& ray, const AABB3D& box) noexcept {
        constexpr auto Huge = 3.0e38f;

        auto enter = 0.0f;
        auto leave = Huge;
        for(usize axis = 0; axis < 3; ++axis) {
            const auto origin = ray.origin[axis];
            const auto direction = ray.direction[axis];
            const auto low = box.center[axis] - box.halfScale[axis];
            const auto high = box.center[axis] + box.halfScale[axis];
            if(direction == 0.0f) {
                if(origin < low || origin > high)
                    return std::nullopt;
                continue;
            }

            const auto t0 = (low - origin) / direction;
            const auto t1 = (high - origin) / direction;
            enter = std::max(enter, std::min(t0, t1));
            leave = std::min(leave, std::max(t0, t1));
            if(enter > leave)
                return std::nullopt;
        }

        return enter;
    }

    // Moller-Trumbore
    inline constexpr std::optional<f32> intersectRayTriangle(
        const Ray3D& ray,
        Vec3 a,
        Vec3 b,
        Vec3 c,
        bool frontOnly
    ) noexcept {
        constexpr auto Epsilon = 1e-12f;

        const auto edge1 = b - a;
        const auto edge2 = c - a;
        // a front's winding normal, cross(edge1, edge2), faces the viewer
        if(frontOnly && dot(ray.direction, cross(edge1, edge2)) >= 0.0f)
            return std::nullopt;

        const auto p = cross(ray.direction, edge2);
        const auto det = dot(edge1, p);
        if(det > -Epsilon && det < Epsilon)
            return std::nullopt;

        const auto inverse = 1.0f / det;
        const auto s = ray.origin - a;
        const auto u = dot(s, p) * inverse;
        if(u < 0.0f || u > 1.0f)
            return std::nullopt;

        const auto q = cross(s, edge1);
        const auto v = dot(ray.direction, q) * inverse;
        if(v < 0.0f || u + v > 1.0f)
            return std::nullopt;

        const auto t = dot(edge2, q) * inverse;
        if(t < 0.0f)
            return std::nullopt;

        return t;
    }
}
