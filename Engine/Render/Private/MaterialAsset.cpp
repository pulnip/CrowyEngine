#include "MaterialAsset.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <utility>

#include "Assert.hpp"
#include "ClassRegistry.hpp"
#include "Expected.hpp"
#include "JsonLoader.hpp"
#include "LogLocal.hpp"
#include "PathUtil.hpp"
#include "StandardFrame.hpp"
#include "StringUtil.hpp"

namespace
{
    constexpr Crowy::StrView BuiltinFolder = "Builtin/";
    constexpr Crowy::i64 MaterialVersion = 2;
    constexpr Crowy::CStr MaterialType = "material";
    constexpr Crowy::CStr MetadataKey = "metadata";

    struct BuiltinMaterial {
        Crowy::StrView name;
        Crowy::MaterialDesc (*make)();
    };

    constexpr std::array<BuiltinMaterial, 4> BuiltinMaterials{{
        {"Opaque", [] { return Crowy::MaterialDesc{}; }},
        {"Translucent",
         [] {
             return Crowy::MaterialDesc{
                 .domain = Crowy::MaterialDomain::Translucent,
                 .data = {.opacity = 0.5f}
             };
         }},
        {"Masked",
         [] {
             return Crowy::MaterialDesc{
                 .domain = Crowy::MaterialDomain::Masked
             };
         }},
        // magenta emission over black, so it reads under any light
        {"Missing", [] {
             return Crowy::MaterialDesc{
                 .data =
                     {.albedo = Crowy::zeros(), .emissive = {1.0f, 0.0f, 1.0f}}
             };
         }},
    }};

    std::expected<Crowy::MaterialDesc, Crowy::Str> readMaterialDesc(
        const Crowy::AssetRoots& roots,
        const Crowy::AssetRef& ref
    ) {
        using namespace Crowy;

        StrView path = ref.Path();
        if(ref.Mount() == AssetMount::Engine &&
           consumePrefix(path, BuiltinFolder)) {
            const auto found = std::ranges::find(
                BuiltinMaterials,
                path,
                &BuiltinMaterial::name
            );
            if(found == BuiltinMaterials.end()) {
                return std::unexpected(
                    std::format(
                        "no built-in material '{}'; the built-ins are {}",
                        path,
                        join(BuiltinMaterials, ", ", &BuiltinMaterial::name)
                    )
                );
            }

            return found->make();
        }

        try {
            return parseMaterialDesc(
                parseJsonFile(resolveAssetPath(roots, ref))
            );
        }
        catch(const std::exception& error) {
            return std::unexpected(Str(error.what()));
        }
    }

    const Crowy::AssetRef& missingTexture() {
        static const auto ref =
            Crowy::AssetRef::Parse("engine:Builtin/Missing").value();
        return ref;
    }

    // a ref the data wrote: a failure is told and sampled as Missing
    Crowy::MaterialTextureAsset loadSlot(
        Crowy::RenderScene& scene,
        Crowy::TextureStore& textures,
        const Crowy::MaterialTexture& slot,
        Crowy::TextureColorSpace space,
        const Crowy::AssetRef& material
    ) {
        using namespace Crowy;

        if(slot.texture.IsNone())
            return {};

        auto loaded = textures.Load(slot.texture);
        if(!loaded) {
            LOG_ERROR(
                "material {}: {}; sampling {} in its place",
                material.ToString(),
                loaded.error(),
                missingTexture().ToString()
            );
            loaded = textures.Load(missingTexture());
        }

        auto asset = valueOrThrow(std::move(loaded));
        auto& texture = *asset->texture;
        const auto row = scene.Textures().Add(
            TextureResource{
                .sampler = slot.sampler,
                .readable = texture.GetReadableID(
                    readViewOf(texture.GetFormat(), space, slot.sampler)
                )
            }
        );

        return {.asset = std::move(asset), .row = row};
    }

    std::expected<std::filesystem::path, Crowy::Str> fileOf(
        const Crowy::AssetRoots& roots,
        const Crowy::AssetRef& ref,
        Crowy::StrView what
    ) {
        using namespace Crowy;

        auto path = resolveAssetPath(roots, ref);
        if(!std::filesystem::is_regular_file(path)) {
            return std::unexpected(
                std::format(
                    "the {} '{}' is no file at {}",
                    what,
                    ref.ToString(),
                    toUTF8String(path)
                )
            );
        }

        return path;
    }
}

namespace Crowy
{
    std::expected<MaterialDesc, Str> parseMaterialDesc(DOM::Value document) {
        if(const auto header =
               checkDocMetadata(document, MaterialType, MaterialVersion);
           !header.empty())
            return std::unexpected(join(header, "\n"));

        document.asTable()->erase(MetadataKey);
        MaterialDesc desc;
        if(const auto errors = applyProperties(&desc, document);
           !errors.empty())
            return std::unexpected(join(errors, "\n"));

        return desc;
    }

    DOM::Value serializeMaterialDesc(const MaterialDesc& desc) {
        DOM::Value properties;
        serializeProperties(&desc, properties);

        DOM::Table metadata;
        metadata.emplace("version", MaterialVersion);
        metadata.emplace("type", MaterialType);

        DOM::Table root;
        root.emplace(MetadataKey, std::move(metadata));
        for(auto& [key, value]: *properties.asTable())
            root.emplace(key, std::move(value));

        return DOM::Value(std::move(root));
    }

    std::expected<MaterialResource, Str> materialResourceOf(
        const MaterialDesc& desc,
        const AssetRoots& roots
    ) {
        MaterialResource material{
            .data = desc.data,
            .pipeline = standardMaterial(
                desc.domain,
                desc.doubleSided ? RHICullMode::None : RHICullMode::Back
            )
        };
        if(!desc.receiveShadow)
            material.data.flags |=
                static_cast<u32>(MaterialFlags::NoShadowReceive);

        auto& pipeline = material.pipeline;
        if(!desc.shader.IsNone()) {
            auto shader = fileOf(roots, desc.shader, "shader");
            if(!shader)
                return std::unexpected(std::move(shader).error());

            pipeline.vertexShader = {.path = *shader, .entryPoint = "vs_main"};
            pipeline.fragmentShader = {
                .path = *shader,
                .entryPoint = "fs_main"
            };
            if(desc.domain == MaterialDomain::Masked)
                pipeline.maskShader = {
                    .path = *shader,
                    .entryPoint = "fs_masked_depth"
                };
        }
        if(!desc.shadingModule.IsNone()) {
            auto shadingModule =
                fileOf(roots, desc.shadingModule, "shading module");
            if(!shadingModule)
                return std::unexpected(std::move(shadingModule).error());

            pipeline.shadingModule = std::move(*shadingModule);
        }

        return material;
    }

    std::expected<MaterialAsset, Str> loadMaterialAsset(
        RenderScene& scene,
        TextureStore& textures,
        const AssetRoots& roots,
        const AssetRef& ref
    ) {
        const auto desc = readMaterialDesc(roots, ref);
        if(!desc)
            return std::unexpected(desc.error());

        auto material = materialResourceOf(*desc, roots);
        if(!material)
            return std::unexpected(std::move(material).error());

        using enum TextureColorSpace;
        MaterialAsset asset{
            .albedo = loadSlot(scene, textures, desc->albedoMap, Srgb, ref),
            .emissive = loadSlot(scene, textures, desc->emissiveMap, Srgb, ref)
        };
        material->maps = {
            .albedo = asset.albedo.row,
            .emissive = asset.emissive.row
        };
        asset.row = scene.Materials().Add(std::move(*material));

        return asset;
    }

    void releaseMaterialAsset(RenderScene& scene, MaterialAsset asset) {
        CROWY_ASSERT(
            scene.Materials().IsValid(asset.row),
            "a material row left behind its store's back, as by "
            "RenderScene::Clear"
        );
        scene.Materials().Remove(asset.row);

        // a retire of the textures may have taken the rows already
        for(const auto row: {asset.albedo.row, asset.emissive.row}) {
            if(scene.Textures().IsValid(row))
                scene.Textures().Remove(row);
        }
    }
}
