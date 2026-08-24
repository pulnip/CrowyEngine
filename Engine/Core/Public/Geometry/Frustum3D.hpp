#pragma once

#include <array>

#include "GeoUtil.hpp"
#include "LinearAlgebra.hpp"
#include "Overlap3D.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct Plane3D {
        // not required to be unit length.
        Vec3 normal = unitY();
        f32 d = 0.0f;

        // Distance >= 0 is inside,
        // otherwise, outside
        auto Distance(const Vec3 point) const noexcept {
            return dot(normal, point) + d;
        }
    };

    inline constexpr auto makePlane3D(Vec4 equation) noexcept {
        return Plane3D{.normal = static_cast<Vec3>(equation), .d = equation.w};
    }

    struct Frustum3D {
        // Left, Right, Bottom, Top, Near, Far
        // (x, y, z order)
        std::array<Plane3D, 6> planes{};
    };

    // Gribb-Hartmann: the six clip-space inequalities,
    inline constexpr auto makeFrustum3D(const Mat4& viewProj) noexcept {
        // clip.i = row(i) * p, and a row of a column-major matrix is a stride
        // walk across the four columns
        const auto row = [&viewProj](usize i) constexpr noexcept {
            return Vec4{
                .x = viewProj[0][i],
                .y = viewProj[1][i],
                .z = viewProj[2][i],
                .w = viewProj[3][i]
            };
        };
        const auto x = row(0), y = row(1), z = row(2), w = row(3);

        return Frustum3D{
            .planes = {
                makePlane3D(w + x), // left:   clip.x >= -clip.w
                makePlane3D(w - x), // right:  clip.x <=  clip.w
                makePlane3D(w + y), // bottom: clip.y >= -clip.w
                makePlane3D(w - y), // top:    clip.y <=  clip.w
                makePlane3D(z),     // near:   clip.z >=  0
                makePlane3D(w - z)  // far:    clip.z <=  clip.w
            }
        };
    }

    inline constexpr bool OverlapFrustumAABB3D(
        const Frustum3D& frustum,
        const AABB3D& box
    ) noexcept {
        for(auto& plane: frustum.planes) {
            // because the normal may point down any of the eight diagonals
            const auto absNormal = abs(plane.normal);

            // the box's extent along the plane normal
            const auto radius = dot(absNormal, box.halfScale);

            if(plane.Distance(box.center) < -radius)
                return false;
        }

        return true;
    }
}
