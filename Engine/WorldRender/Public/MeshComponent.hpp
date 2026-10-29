#pragma once

#include <vector>

#include "AssetRef.hpp"
#include "MaterialAsset.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    // A mesh asset drawn with the material of each slot's index; a slot past
    // the list, or given no ref, draws engine:Builtin/Opaque.
    struct MeshComponent {
        AssetRef mesh;
        std::vector<AssetRef> materials;
        bool visible = true;
        bool castShadow = true;

        // the refs the rows below were built from; a write of the refs above
        // builds them again at the next Sync
        AssetRef builtMesh;
        std::vector<AssetRef> builtMaterials;
        MaterialAssetHandles materialAssets;
        MeshHandle resource;
        PrimitiveHandle row;
    };
}
