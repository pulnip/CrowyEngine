#pragma once

#include <functional>
#include <optional>
#include <span>

#include "EditorCamera.hpp"
#include "Geometry/Ray3D.hpp"
#include "MeshData.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    struct PickHit;

    using MeshList = std::span<const MeshData* const>;
    // the meshes a primitive draws, in its model space; empty for one the
    // content does not let the editor pick
    using PickMeshes = std::function<MeshList(PrimitiveHandle)>;

    // a world point in window points from the top-left, or nothing behind
    // the near plane
    inline constexpr std::optional<Vec2> projectToWindow(const Mat4& viewProj, Vec3 point, Vec2 viewport) {
        const auto clip = viewProj * Vec4{point.x, point.y, point.z, 1.0f};
        if(!(clip.z >= 0.0f && clip.w > 0.0f))
            return std::nullopt;

        return Vec2{
            (clip.x / clip.w + 1.0f) * 0.5f * viewport.x,
            (1.0f - clip.y / clip.w) * 0.5f * viewport.y
        };
    }

    // the point drawn nearest `pixel`, within `radius` window points
    inline constexpr std::optional<usize> nearestOnScreen(
        const Mat4& viewProj,
        Vec2 viewport,
        Vec2 pixel,
        std::span<const Vec3> points,
        f32 radius
    ) {
        std::optional<usize> nearest;
        auto best = radius * radius;
        for(usize i = 0; i < points.size(); ++i) {
            const auto at = projectToWindow(viewProj, points[i], viewport);
            if(!at)
                continue;

            const auto offset = *at - pixel;
            const auto distance = dot(offset, offset);
            if(distance <= best) {
                best = distance;
                nearest = i;
            }
        }

        return nearest;
    }

    // the ray from a pixel (window points from the top-left) through the
    // camera, either lens; nothing for a viewport without area
    std::optional<Ray3D> rayThroughPixel(const EditorCamera& camera, Vec2 pixel, Vec2 viewport);

    // the nearest front-facing triangle of a visible primitive along the ray,
    // testing boxes nearest first and stopping past the best hit
    std::optional<PickHit> pickScene(const RenderScene& scene, const Ray3D& ray, const PickMeshes& meshes);

    struct PickHit {
        PrimitiveHandle primitive;
        // along the ray, in its direction's units
        f32 distance = 0.0f;
    };
}
