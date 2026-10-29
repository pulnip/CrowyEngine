#include "PrimitiveRow.hpp"

#include "Geometry/Overlap3D.hpp"

namespace Crowy
{
    void writePrimitiveRow(
        const World& world,
        ActorId actor,
        RenderScene& scene,
        PrimitiveHandle row,
        MeshHandle mesh,
        PrimitiveFlags flags
    ) {
        auto& snapshot = scene.Primitives().GetRef(row);
        snapshot.localToWorld = world.WorldMatrix(actor);
        snapshot.worldBounds = transformAABB3D(
            snapshot.localToWorld,
            scene.Meshes().GetRef(mesh).localBounds
        );
        snapshot.mesh = mesh;
        snapshot.flags = flags;
    }
}
