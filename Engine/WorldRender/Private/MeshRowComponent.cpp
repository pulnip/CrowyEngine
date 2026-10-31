#include "MeshRowComponent.hpp"

#include "Component.hpp"
#include "PrimitiveActors.hpp"
#include "PrimitiveRow.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        void addRow(World& world, ActorId actor, MeshRowComponent& mesh) {
            auto& scene = world.Service<RenderScene>();
            auto& prims = scene.Primitives();

            mesh.row = prims.Add(
                PrimitiveSnapshot{.mesh = mesh.mesh, .flags = mesh.flags}
            );
            if(auto* actors = world.FindService<PrimitiveActors>())
                actors->Bind(mesh.row, actor);
        }

        void removeRow(World& world, ActorId, MeshRowComponent& mesh) {
            auto& scene = world.Service<RenderScene>();
            auto& prims = scene.Primitives();

            if(auto* actors = world.FindService<PrimitiveActors>())
                actors->Unbind(mesh.row);
            prims.Remove(mesh.row);
        }

        void extractMeshRows(World& world, const FrameTime&) {
            // asked for on the first mesh, so a World without one needs no
            // RenderScene
            RenderScene* scene = nullptr;
            world.EachChanged<MeshRowComponent>(
                [&](ActorId actor, const MeshRowComponent& mesh) {
                    if(scene == nullptr)
                        scene = &world.Service<RenderScene>();

                    writePrimitiveRow(
                        world,
                        actor,
                        *scene,
                        mesh.row,
                        mesh.mesh,
                        mesh.flags
                    );
                }
            );
        }
    }

    // clang-format off
    CROWY_COMPONENT(MeshRowComponent)
        .OnAdd(&addRow)
        .OnRemove(&removeRow)
    CROWY_COMPONENT_END(MeshRowComponent)

    CROWY_SYSTEM(ExtractMeshRows, Extract, &extractMeshRows)
    CROWY_SYSTEM_END(ExtractMeshRows)
    // clang-format on
}
