#pragma once

#include <optional>
#include <span>

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct CameraLens;

    inline Mat4 projectionOf(const CameraLens& lens, f32 aspect) noexcept;

    // a clip-space point with w > 0 in window points from the top-left
    inline constexpr Vec2 clipToWindow(Vec4 clip, Vec2 viewport) noexcept {
        return Vec2{
            (clip.x / clip.w + 1.0f) * 0.5f * viewport.x,
            (1.0f - clip.y / clip.w) * 0.5f * viewport.y
        };
    }

    // nullopt behind the near plane
    inline constexpr std::optional<Vec2> projectToWindow(
        const Mat4& viewProj,
        Vec3 point,
        Vec2 viewport
    ) noexcept {
        const auto clip = viewProj * toVec4(point, 1.0f);
        if(!(clip.z >= 0.0f && clip.w > 0.0f))
            return std::nullopt;

        return clipToWindow(clip, viewport);
    }

    inline constexpr std::optional<usize> nearestOnScreen(
        const Mat4& viewProj,
        Vec2 viewport,
        Vec2 pixel,
        std::span<const Vec3> points,
        f32 radius
    ) noexcept {
        std::optional<usize> nearest;
        auto best = radius * radius;
        for(usize i = 0; i < points.size(); ++i) {
            const auto at = projectToWindow(viewProj, points[i], viewport);
            if(!at)
                continue;

            const auto distance = normSquared(*at - pixel);
            if(distance <= best) {
                best = distance;
                nearest = i;
            }
        }

        return nearest;
    }

    // the part of a clip-space segment in front of the near plane (z >= 0);
    // false when none is
    inline constexpr bool clipSegmentToNear(Vec4& a, Vec4& b) noexcept {
        if(a.z < 0.0f && b.z < 0.0f)
            return false;
        if(a.z < 0.0f)
            a = a + (b - a) * (a.z / (a.z - b.z));
        else if(b.z < 0.0f)
            b = b + (a - b) * (b.z / (b.z - a.z));

        return true;
    }

    struct CameraLens {
        // vertical, radians
        f32 fovY = 1.0f;
        f32 nearZ = 0.3f;
        f32 farZ = 1000.0f;
        bool orthographic = false;
        // half the vertical extent in meters, for an orthographic lens
        f32 orthoHalfHeight = 10.0f;

        bool operator==(const CameraLens&) const = default;
    };

    inline Mat4 projectionOf(const CameraLens& lens, f32 aspect) noexcept {
        if(lens.orthographic) {
            const auto height = 2.0f * lens.orthoHalfHeight;
            return orthographic(height * aspect, height, lens.nearZ, lens.farZ);
        }

        return perspective(lens.fovY, aspect, lens.nearZ, lens.farZ);
    }
}
