#pragma once

#include <vector>

namespace Crowy
{
    struct SceneSpec;
    class EntityRegistry;

    void loadScene(
        const SceneSpec& scene,
        EntityRegistry& registry
    );
}