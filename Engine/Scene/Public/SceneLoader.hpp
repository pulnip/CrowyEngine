#pragma once

#include <vector>

namespace Crowy
{
    struct SceneSpec;
    class EntityRegistry;
    struct ResourceHub;

    void loadScene(
        const SceneSpec& scene,
        EntityRegistry& registry,
        ResourceHub& hub
    );
}