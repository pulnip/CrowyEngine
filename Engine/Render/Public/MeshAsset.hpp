#pragma once

#include <expected>
#include <vector>

#include "AssetRoots.hpp"
#include "AssetStore.hpp"
#include "Geometry/Overlap3D.hpp"
#include "GeometryPool.hpp"
#include "MeshData.hpp"
#include "ModelLoader.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct MeshAsset;
    struct MeshAssetSlot;

    using MeshStore = AssetStore<MeshAsset>;
    using MeshAssetHandle = AssetHandle<MeshAsset>;
    using MeshAssetSlots = std::vector<MeshAssetSlot>;

    // engine:Builtin/Box, Sphere and Plane come from code, never from disk;
    // any other ref is a model file under its mount
    std::expected<MeshAsset, Str> loadMeshAsset(
        GeometryPool& pool,
        const AssetRoots& roots,
        const AssetRef& ref
    );
    // a decoded model's slots into the pool; when one does not fit, the ones
    // that did go back
    std::expected<MeshAsset, Str> uploadMeshAsset(
        GeometryPool& pool,
        ModelData model
    );
    // the ranges come back once the GPU is done with them
    void releaseMeshAsset(GeometryPool& pool, MeshAsset asset);

    // the triangles of one material, in the pool and on the CPU
    struct MeshAssetSlot {
        // kept for picking, as an editor reads triangles on the CPU
        MeshData mesh;
        GeometryAllocation geometry;
        // as the file names the material; empty for faces that have none
        Str material;
    };

    struct MeshAsset {
        MeshAssetSlots slots;
        AABB3D bounds;
    };
}
