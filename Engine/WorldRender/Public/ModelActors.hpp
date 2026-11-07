#pragma once

#include <vector>

#include "ModelLoader.hpp"
#include "World.hpp"

namespace Crowy
{
    // an actor per node of the model, named and placed as the file has it,
    // under its parent node's actor, the top ones under `parent`; by node
    std::vector<ActorId> spawnModelActors(
        World& world,
        const ModelData& model,
        ActorId parent = ActorId::InvalidHandle()
    );
}
