#pragma once

#include <vector>
#include "ECSDefinitions.hpp"

namespace Crowy
{
    struct SceneSpec;
    class EntityRegistry;

    std::vector<EntityID> instantiateScene(
        const SceneSpec& scene,
        EntityRegistry& registry
    );
}