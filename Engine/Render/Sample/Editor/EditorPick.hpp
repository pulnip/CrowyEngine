#pragma once

#include <functional>
#include <optional>
#include <span>

#include "EditorCamera.hpp"
#include "Geometry/Projection.hpp"
#include "Geometry/Ray3D.hpp"
#include "MeshData.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    struct PickHit;

    using MeshList = std::span<const MeshData* const>;
    // in model space; empty for a primitive the content keeps from picking
    using PickMeshes = std::function<MeshList(PrimitiveHandle)>;

    inline constexpr Vec2 EditorNoPick{-1.0f, -1.0f};

    std::optional<Ray3D> rayThroughPixel(
        const EditorCamera& camera,
        Vec2 pixel,
        Vec2 viewport
    );

    std::optional<PickHit> pickScene(
        const RenderScene& scene,
        const Ray3D& ray,
        const PickMeshes& meshes
    );

    struct PickHit {
        PrimitiveHandle primitive;
        // along the ray, in its direction's units
        f32 distance = 0.0f;
    };
}
