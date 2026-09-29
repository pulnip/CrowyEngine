#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "Geometry/Overlap3D.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

// The directional shadow's view, device-free. The struct comes before the
// function because the function returns it.
namespace Crowy
{
    struct DirectionalShadowFit {
        // world to the shadow view's clip space; view row 1's viewProj
        Mat4 worldToShadow = unitMat();
        // world metres per texel, the same along both axes
        f32 texelSize = 0.0f;
        // the light-space depths clip z 0 and 1 stand for
        f32 nearZ = 0.0f;
        f32 farZ = 0.0f;
    };

    // an orthographic view down `travel` holding `bounds` whole, square texels
    inline DirectionalShadowFit fitDirectionalShadow(
        Vec3 travel,
        const AABB3D& bounds,
        u32 mapSize
    ) {
        // a point-sized scene still gets a map with a size
        constexpr f32 MinExtent = 0.01f;
        constexpr f32 Infinity = std::numeric_limits<f32>::infinity();

        const auto forward = normalize(travel);
        // lookAt has no answer for an up along the forward axis
        const auto up = std::abs(forward.y) > 0.99f ? unitZ() : unitY();
        const auto lightView = lookAt(zeros(), forward, up);

        Vec3 low{Infinity, Infinity, Infinity};
        Vec3 high{-Infinity, -Infinity, -Infinity};
        for(u32 corner = 0; corner < 8; ++corner) {
            const Vec3 side{
                (corner & 1u) != 0 ? 1.0f : -1.0f,
                (corner & 2u) != 0 ? 1.0f : -1.0f,
                (corner & 4u) != 0 ? 1.0f : -1.0f
            };
            const auto p = static_cast<Vec3>(
                lightView * toVec4(bounds.center + side * bounds.halfScale, 1.0f)
            );
            for(usize i = 0; i < 3; ++i) {
                low[i] = std::min(low[i], p[i]);
                high[i] = std::max(high[i], p[i]);
            }
        }

        // square, so a bias in texels has one world size
        const auto extent =
            std::max({high.x - low.x, high.y - low.y, MinExtent});
        const auto texelSize = extent / static_cast<f32>(mapSize);
        // a texel of margin, so no caster clips on rounding
        const auto nearZ = low.z - texelSize;
        const auto farZ = high.z + texelSize;
        // orthographic is centred, so the box's centre moves onto the axis
        const auto centre = 0.5f * (low + high);

        return DirectionalShadowFit{
            .worldToShadow = orthographic(extent, extent, nearZ, farZ) *
                translateMat({-centre.x, -centre.y, 0.0f}) * lightView,
            .texelSize = texelSize,
            .nearZ = nearZ,
            .farZ = farZ
        };
    }
}
