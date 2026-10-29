#pragma once

#include "RenderScene.hpp"
#include "World.hpp"

namespace Crowy
{
    // the row a mesh drawn at the actor is extracted into
    void writePrimitiveRow(
        const World& world,
        ActorId actor,
        RenderScene& scene,
        PrimitiveHandle row,
        MeshHandle mesh,
        PrimitiveFlags flags
    );
}
