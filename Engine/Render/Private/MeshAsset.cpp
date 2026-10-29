#include "MeshAsset.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <format>
#include <utility>

#include "MeshGenerator.hpp"
#include "ModelLoader.hpp"
#include "StringUtil.hpp"

namespace
{
    constexpr Crowy::StrView BuiltinFolder = "Builtin/";

    struct BuiltinMesh {
        Crowy::StrView name;
        Crowy::MeshData (*make)();
    };

    constexpr std::array<BuiltinMesh, 4> BuiltinMeshes{{
        {"Box", [] { return Crowy::MakeBox(0.5f); }},
        {"Sphere", [] { return Crowy::MakeSphere(0.5f, 32, 16); }},
        {"Plane", [] { return Crowy::MakePlane(Crowy::Vec2{0.5f, 0.5f}); }},
        // what a reference that fails to load draws instead
        {"Missing", [] { return Crowy::MakeBox(0.5f); }},
    }};

    std::expected<Crowy::MeshData, Crowy::Str> builtinMesh(
        Crowy::StrView name
    ) {
        using namespace Crowy;

        const auto found =
            std::ranges::find(BuiltinMeshes, name, &BuiltinMesh::name);
        if(found == BuiltinMeshes.end()) {
            return std::unexpected(
                std::format(
                    "no built-in mesh '{}'; the built-ins are {}",
                    name,
                    join(BuiltinMeshes, ", ", &BuiltinMesh::name)
                )
            );
        }

        return found->make();
    }

    std::expected<Crowy::ModelData, Crowy::Str> readModel(
        const Crowy::AssetRoots& roots,
        const Crowy::AssetRef& ref
    ) {
        using namespace Crowy;

        StrView path = ref.Path();
        if(ref.Mount() == AssetMount::Engine &&
           consumePrefix(path, BuiltinFolder)) {
            auto mesh = builtinMesh(path);
            if(!mesh)
                return std::unexpected(std::move(mesh).error());

            ModelSlot slot{.mesh = std::move(*mesh)};
            slot.bounds = boundsOf(slot.mesh.vertices);
            ModelData model{.bounds = slot.bounds};
            model.slots.push_back(std::move(slot));
            return model;
        }

        try {
            return loadModel(resolveAssetPath(roots, ref));
        }
        catch(const std::exception& error) {
            return std::unexpected(Str(error.what()));
        }
    }
}

namespace Crowy
{
    std::expected<MeshAsset, Str> loadMeshAsset(
        GeometryPool& pool,
        const AssetRoots& roots,
        const AssetRef& ref
    ) {
        auto model = readModel(roots, ref);
        if(!model)
            return std::unexpected(std::move(model).error());

        return uploadMeshAsset(pool, std::move(*model));
    }

    std::expected<MeshAsset, Str> uploadMeshAsset(
        GeometryPool& pool,
        ModelData model
    ) {
        if(model.slots.empty())
            return std::unexpected(Str("the model holds no triangles"));

        MeshAsset asset{.bounds = model.bounds};
        for(auto& slot: model.slots) {
            const auto geometry =
                pool.Add(slot.mesh.vertices, slot.mesh.indices);
            if(!geometry) {
                releaseMeshAsset(pool, std::move(asset));
                return std::unexpected(geometry.error());
            }

            asset.slots.push_back(
                MeshAssetSlot{
                    .mesh = std::move(slot.mesh),
                    .geometry = *geometry,
                    .material = std::move(slot.material)
                }
            );
        }

        return asset;
    }

    void releaseMeshAsset(GeometryPool& pool, MeshAsset asset) {
        for(const auto& slot: asset.slots)
            pool.Free(slot.geometry);
    }
}
