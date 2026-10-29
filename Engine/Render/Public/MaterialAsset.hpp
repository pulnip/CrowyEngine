#pragma once

#include <expected>
#include <vector>

#include "AssetStore.hpp"
#include "Primitives.hpp"
#include "RenderMaterial.hpp"

namespace Crowy
{
    struct MaterialAsset;

    using MaterialStore = AssetStore<MaterialAsset>;
    using MaterialAssetHandle = AssetHandle<MaterialAsset>;
    using MaterialAssetHandles = std::vector<MaterialAssetHandle>;

    // the engine:Builtin/ presets only: Opaque, Translucent, Masked, Missing
    std::expected<MaterialAsset, Str> loadMaterialAsset(
        MaterialTable& materials,
        const AssetRef& ref
    );
    void releaseMaterialAsset(MaterialTable& materials, MaterialAsset asset);

    // one row of the material table, shared by every mesh that draws it
    struct MaterialAsset {
        MaterialHandle row;
    };
}
