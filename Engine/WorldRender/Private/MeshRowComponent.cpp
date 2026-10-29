#include "MeshRowComponent.hpp"

#include "Component.hpp"
#include "Geometry/Overlap3D.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        void addRow(World& world, ActorId, MeshRowComponent& mesh) {
            auto& scene = world.Service<RenderScene>();
            auto& prims = scene.Primitives();

            mesh.row = prims.Add(
                PrimitiveSnapshot{.mesh = mesh.mesh, .flags = mesh.flags}
            );
        }

        void removeRow(World& world, ActorId, MeshRowComponent& mesh) {
            auto& scene = world.Service<RenderScene>();
            auto& prims = scene.Primitives();

            prims.Remove(mesh.row);
        }

        void extractMeshes(World& world, const FrameTime&) {
            // asked for on the first mesh, so a World without one needs no
            // RenderScene
            RenderScene* scene = nullptr;
            world.EachChanged<MeshRowComponent>(
                [&](ActorId actor, const MeshRowComponent& mesh) {
                    if(scene == nullptr)
                        scene = &world.Service<RenderScene>();
                    auto& prims = scene->Primitives();
                    auto& meshes = scene->Meshes();

                    auto& row = prims.GetRef(mesh.row);
                    row.localToWorld = world.WorldMatrix(actor);
                    row.worldBounds = transformAABB3D(
                        row.localToWorld,
                        meshes.GetRef(mesh.mesh).localBounds
                    );
                    row.mesh = mesh.mesh;
                    row.flags = mesh.flags;
                }
            );
        }
    }

    // clang-format off
    CROWY_COMPONENT(MeshRowComponent)
        .OnAdd(&addRow)
        .OnRemove(&removeRow)
    CROWY_COMPONENT_END(MeshRowComponent)

    CROWY_SYSTEM(ExtractMeshes, Extract, &extractMeshes)
    CROWY_SYSTEM_END(ExtractMeshes)
    // clang-format on
}
