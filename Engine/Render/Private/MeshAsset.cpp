#include "MeshAsset.hpp"

#include <exception>
#include <format>
#include <utility>

#include "MeshGenerator.hpp"
#include "ModelLoader.hpp"

namespace
{
    constexpr Crowy::StrView BuiltinFolder = "Builtin/";

    std::expected<Crowy::MeshData, Crowy::Str> builtinMesh(
        Crowy::StrView name
    ) {
        using namespace Crowy;

        if(name == "Box")
            return MakeBox(0.5f);
        if(name == "Sphere")
            return MakeSphere(0.5f, 32, 16);
        if(name == "Plane")
            return MakePlane(Vec2{0.5f, 0.5f});

        return std::unexpected(std::format("no built-in mesh '{}'", name));
    }

    std::expected<Crowy::ModelData, Crowy::Str> readModel(
        const Crowy::AssetRoots& roots,
        const Crowy::AssetRef& ref
    ) {
        using namespace Crowy;

        const StrView path = ref.Path();
        if(ref.Mount() == AssetMount::Engine &&
           path.starts_with(BuiltinFolder)) {
            auto mesh = builtinMesh(path.substr(BuiltinFolder.size()));
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
