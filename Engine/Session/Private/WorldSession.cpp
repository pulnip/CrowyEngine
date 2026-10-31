#include "WorldSession.hpp"

#include <utility>

#include "Scene.hpp"

namespace Crowy
{
    WorldSession::WorldSession(
        const SystemRegistry& systems,
        RHIDevice& device,
        GeometryPool& pool,
        const AssetRoots& roots
    )
        : render(device, pool, roots), world(systems) {
        world.Provide(render.Scene());
        world.Provide(render.Meshes());
        world.Provide(render.Materials());
        world.Provide(primitiveActors);
        world.Provide(camera);
    }

    void WorldSession::Load(const std::filesystem::path& file) {
        loadSceneFile(world, file);
        world.Frame(0.0);
    }

    void WorldSession::Load(DOM::Value scene) {
        loadScene(world, std::move(scene));
        world.Frame(0.0);
    }
}
