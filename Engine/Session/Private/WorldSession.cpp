#include "WorldSession.hpp"

#include <utility>

#include "Scene.hpp"

namespace Crowy
{
    WorldSession::WorldSession(
        RAII<RenderSession> render,
        const SystemRegistry& systems
    )
        : render(std::move(render)), world(systems) {
        world.Provide(Render().Scene());
        world.Provide(Render().Meshes());
        world.Provide(Render().Materials());
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
