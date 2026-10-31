#pragma once

#include <expected>
#include <vector>

#include "AssetRoots.hpp"
#include "AssetStore.hpp"
#include "DOM.hpp"
#include "Primitives.hpp"
#include "RenderMaterial.hpp"
#include "RenderScene.hpp"
#include "RenderTexture.hpp"
#include "TextureAsset.hpp"

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
    // other ref is a material file. A texture it names that fails to load is
    // told and sampled as engine:Builtin/Missing.
    std::expected<MaterialAsset, Str> loadMaterialAsset(
        RenderScene& scene,
        TextureStore& textures,
        const AssetRoots& roots,
        const AssetRef& ref
    );
    void releaseMaterialAsset(RenderScene& scene, MaterialAsset asset);

    // a texture a slot samples, and how; colors are read as sRGB
    struct MaterialTexture {
        AssetRef texture;
        TextureSampler sampler = TextureSampler::LinearWrap;
    };

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
        MaterialTexture albedoMap;
        MaterialTexture emissiveMap;
    };

    // a slot's texture, held, and its row in the scene's texture table
    struct MaterialTextureAsset {
        TextureAssetHandle asset;
        TextureHandle row;
    };

    // one row of the material table, shared by every mesh that draws it
    struct MaterialAsset {
        MaterialHandle row;
        MaterialTextureAsset albedo;
        MaterialTextureAsset emissive;
    };
}
