#pragma once

#include <expected>
#include <vector>

#include "AssetRoots.hpp"
#include "AssetStore.hpp"
#include "DOM.hpp"
#include "Primitives.hpp"
#include "RenderMaterial.hpp"

namespace Crowy
{
    struct MaterialDesc;
    struct MaterialAsset;

    using MaterialStore = AssetStore<MaterialAsset>;
    using MaterialAssetHandle = AssetHandle<MaterialAsset>;
    using MaterialAssetHandles = std::vector<MaterialAssetHandle>;

    std::expected<MaterialDesc, Str> parseMaterialDesc(DOM::Value document);
    DOM::Value serializeMaterialDesc(const MaterialDesc& desc);
    // a shader or shading module the desc names must be on disk
    std::expected<MaterialResource, Str> materialResourceOf(
        const MaterialDesc& desc,
        const AssetRoots& roots
    );

    // engine:Builtin/ holds Opaque, Translucent, Masked and Missing; any
    // other ref is a material file
    std::expected<MaterialAsset, Str> loadMaterialAsset(
        MaterialTable& materials,
        const AssetRoots& roots,
        const AssetRef& ref
    );
    void releaseMaterialAsset(MaterialTable& materials, MaterialAsset asset);

    // a material file: the pipeline a row draws with and the values it carries
    struct MaterialDesc {
        MaterialDomain domain = MaterialDomain::Opaque;
        // a Slang file that exports vs_main and fs_main, and fs_masked_depth
        // when Masked; none draws the standard forward shader
        AssetRef shader;
        // a shading module; none links PBR
        AssetRef shadingModule;
        bool doubleSided = false;
        bool receiveShadow = true;
        MaterialData data{};
    };

    // one row of the material table, shared by every mesh that draws it
    struct MaterialAsset {
        MaterialHandle row;
    };
}
