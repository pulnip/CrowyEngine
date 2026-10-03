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

    // the meshes a primitive draws, in its model space; empty for one the
    // content does not let the editor pick
    using PickMeshes = std::function<std::span<const MeshData* const>(PrimitiveHandle)>;

    // the ray from a pixel (window points from the top-left) through the
    // camera, either lens
    Ray3D rayThroughPixel(const EditorCamera& camera, Vec2 pixel, Vec2 viewport);

    // the nearest front-facing triangle of a visible primitive along the ray:
    // boxes nearest first, triangles only for boxes the ray enters before the
    // best hit so far
    std::optional<PickHit> pickScene(const RenderScene& scene, const Ray3D& ray, const PickMeshes& meshes);

    struct PickHit {
        PrimitiveHandle primitive;
        // along the ray, in its direction's units
        f32 distance = 0.0f;
    };
}
