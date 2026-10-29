#include "MaterialAsset.hpp"

#include <algorithm>
#include <array>
#include <format>

#include "Assert.hpp"
#include "StandardFrame.hpp"
#include "StringUtil.hpp"

namespace
{
    constexpr Crowy::StrView BuiltinFolder = "Builtin/";

    struct BuiltinMaterial {
        Crowy::StrView name;
        Crowy::MaterialResource (*make)();
    };

    constexpr std::array<BuiltinMaterial, 4> BuiltinMaterials{{
        {"Opaque",
         [] {
             return Crowy::MaterialResource{
                 .pipeline =
                     Crowy::standardMaterial(Crowy::MaterialDomain::Opaque)
             };
         }},
        {"Translucent",
         [] {
             return Crowy::MaterialResource{
                 .data = {.opacity = 0.5f},
                 .pipeline =
                     Crowy::standardMaterial(Crowy::MaterialDomain::Translucent)
             };
         }},
        {"Masked",
         [] {
             return Crowy::MaterialResource{
                 .pipeline =
                     Crowy::standardMaterial(Crowy::MaterialDomain::Masked)
             };
         }},
        // magenta emission over black, so it reads under any light
        {"Missing", [] {
             return Crowy::MaterialResource{
                 .data =
                     {.albedo = Crowy::zeros(), .emissive = {1.0f, 0.0f, 1.0f}},
                 .pipeline =
                     Crowy::standardMaterial(Crowy::MaterialDomain::Opaque)
             };
         }},
    }};
}

namespace Crowy
{
    std::expected<MaterialAsset, Str> loadMaterialAsset(
        MaterialTable& materials,
        const AssetRef& ref
    ) {
        StrView path = ref.Path();
        if(ref.Mount() != AssetMount::Engine ||
           !consumePrefix(path, BuiltinFolder)) {
            return std::unexpected(
                Str("no material file loads yet; the built-ins are "
                    "engine:Builtin/<name>")
            );
        }

        const auto found =
            std::ranges::find(BuiltinMaterials, path, &BuiltinMaterial::name);
        if(found == BuiltinMaterials.end()) {
            return std::unexpected(
                std::format(
                    "no built-in material '{}'; the built-ins are {}",
                    path,
                    join(BuiltinMaterials, ", ", &BuiltinMaterial::name)
                )
            );
        }

        return MaterialAsset{.row = materials.Add(found->make())};
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
