#include "MaterialAsset.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <utility>

#include "Assert.hpp"
#include "ClassRegistry.hpp"
#include "JsonLoader.hpp"
#include "PathUtil.hpp"
#include "StandardFrame.hpp"
#include "StringUtil.hpp"

namespace
{
    constexpr Crowy::StrView BuiltinFolder = "Builtin/";
    constexpr Crowy::i64 MaterialVersion = 1;
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
        MaterialTable& materials,
        const AssetRoots& roots,
        const AssetRef& ref
    ) {
        const auto desc = readMaterialDesc(roots, ref);
        if(!desc)
            return std::unexpected(desc.error());

        auto material = materialResourceOf(*desc, roots);
        if(!material)
            return std::unexpected(std::move(material).error());

        return MaterialAsset{.row = materials.Add(std::move(*material))};
    }

    void releaseMaterialAsset(MaterialTable& materials, MaterialAsset asset) {
        CROWY_ASSERT(
            materials.IsValid(asset.row),
            "a material row left behind its store's back, as by "
            "RenderScene::Clear"
        );
        materials.Remove(asset.row);
    }
}
