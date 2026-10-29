#include "MeshComponent.hpp"

#include <utility>

#include "Component.hpp"
#include "Expected.hpp"
#include "Log.hpp"
#include "MeshAsset.hpp"
#include "PrimitiveRow.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        const AssetRef& opaqueMaterial() {
            static const auto ref =
                AssetRef::Parse("engine:Builtin/Opaque").value();
            return ref;
        }

        // a mesh and a material alike
        const AssetRef& missingAsset() {
            static const auto ref =
                AssetRef::Parse("engine:Builtin/Missing").value();
            return ref;
        }

        PrimitiveFlags flagsOf(const MeshComponent& mesh) {
            auto flags = PrimitiveFlags::None;
            if(mesh.visible)
                flags = combine(flags, PrimitiveFlags::Visible);
            if(mesh.castShadow)
                flags = combine(flags, PrimitiveFlags::CastShadow);

            return flags;
        }

        // a ref the data wrote: a failure is told and drawn as Missing
        template<typename T>
        AssetHandle<T> loadOrMissing(
            AssetStore<T>& store,
            const AssetRef& ref,
            const AssetRef& missing,
            i64 actorId
        ) {
            auto loaded = store.Load(ref);
            if(loaded)
                return std::move(*loaded);

            LOG_ERROR(
                "WorldRender",
                "actor {}: {}; drawing {} in its place",
                actorId,
                loaded.error(),
                missing.ToString()
            );
            return valueOrThrow(store.Load(missing));
        }

        void forgetBuilt(MeshComponent& mesh) {
            mesh.builtMesh = {};
            mesh.builtMaterials.clear();
            mesh.materialAssets.clear();
            mesh.resource = {};
            mesh.row = {};
        }

        void releaseRows(RenderScene& scene, MeshComponent& mesh) {
            if(mesh.row.IsValid()) {
                scene.Primitives().Remove(mesh.row);
                scene.Meshes().Remove(mesh.resource);
            }
            forgetBuilt(mesh);
        }

        void buildRows(World& world, ActorId actor, MeshComponent& mesh) {
            auto& scene = world.Service<RenderScene>();
            releaseRows(scene, mesh);

            mesh.builtMesh = mesh.mesh;
            mesh.builtMaterials = mesh.materials;
            if(mesh.mesh.IsNone())
                return;

            const auto actorId = world.FileId(actor);
            auto asset = loadOrMissing(
                world.Service<MeshStore>(),
                mesh.mesh,
                missingAsset(),
                actorId
            );
            const auto slots = asset->slots.size();
            if(mesh.materials.size() > slots) {
                LOG_WARN(
                    "WorldRender",
                    "actor {}: {} materials for the {} slots of {}; the rest "
                    "draw nothing",
                    actorId,
                    mesh.materials.size(),
                    slots,
                    asset.Ref().ToString()
                );
            }

            auto& materials = world.Service<MaterialStore>();
            MaterialHandles rows;
            for(usize slot = 0; slot < slots; ++slot) {
                const bool given = slot < mesh.materials.size() &&
                                   !mesh.materials[slot].IsNone();
                auto material = loadOrMissing(
                    materials,
                    given ? mesh.materials[slot] : opaqueMaterial(),
                    missingAsset(),
                    actorId
                );

                rows.push_back(material->row);
                mesh.materialAssets.push_back(std::move(material));
            }

            mesh.resource = scene.Meshes().Add(
                meshResourceOf(std::move(asset), std::move(rows))
            );
            mesh.row = scene.Primitives().Add(
                PrimitiveSnapshot{.mesh = mesh.resource, .flags = flagsOf(mesh)}
            );
        }

        // a copy of a built component would share its rows
        void forgetRows(World&, ActorId, MeshComponent& mesh) {
            forgetBuilt(mesh);
        }

        void removeRows(World& world, ActorId, MeshComponent& mesh) {
            releaseRows(world.Service<RenderScene>(), mesh);
        }

        void buildMeshes(World& world, const FrameTime&) {
            world.EachChanged<MeshComponent>([&](ActorId actor,
                                                 const MeshComponent& mesh) {
                if(mesh.mesh == mesh.builtMesh &&
                   mesh.materials == mesh.builtMaterials)
                    return;

                buildRows(world, actor, *world.Write<MeshComponent>(actor));
            });
        }

        void extractMeshes(World& world, const FrameTime&) {
            // asked for on the first mesh, so a World without one needs no
            // RenderScene
            RenderScene* scene = nullptr;
            world.EachChanged<MeshComponent>([&](ActorId actor,
                                                 const MeshComponent& mesh) {
                if(!mesh.row.IsValid())
                    return;
                if(scene == nullptr)
                    scene = &world.Service<RenderScene>();

                writePrimitiveRow(
                    world,
                    actor,
                    *scene,
                    mesh.row,
                    mesh.resource,
                    flagsOf(mesh)
                );
            });
        }
    }

    // clang-format off
    CROWY_COMPONENT(MeshComponent)
        .AddProperty("mesh", &MeshComponent::mesh)
        .AddProperty("materials", &MeshComponent::materials)
        .AddProperty("visible", &MeshComponent::visible)
        .AddProperty("castShadow", &MeshComponent::castShadow)
        .OnAdd(&forgetRows)
        .OnRemove(&removeRows)
    CROWY_COMPONENT_END(MeshComponent)

    CROWY_SYSTEM(BuildMeshes, Sync, &buildMeshes)
    CROWY_SYSTEM_END(BuildMeshes)

    CROWY_SYSTEM(ExtractMeshes, Extract, &extractMeshes)
    CROWY_SYSTEM_END(ExtractMeshes)
    // clang-format on
}
