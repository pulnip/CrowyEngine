#include <memory>

#include "ProjectRoot.hpp"
#include "RenderApp.hpp"
#include "Scene.hpp"
#include "SnapshotCamera.hpp"
#include "World.hpp"

namespace Crowy
{
    // a scene file of built-in meshes and materials, drawn from its camera
    // actor and lit by its environment actor; nothing in it is made by code
    class HelloScene: public RenderApp {
        World world;

    public:
        HelloScene()
            : RenderApp(Config{}, std::make_unique<SnapshotCamera>()) {}

    protected:
        void OnBuildGeometry(GeometryPool&) override {}

        void ExtractScene(RenderScene& scene) override {
            world.Provide(scene);
            world.Provide(Meshes());
            world.Provide(Materials());
            world.Provide(static_cast<SnapshotCamera&>(Camera()).Snapshot());

            loadSceneFile(
                world,
                resolveProjectPath("Engine/Scene/Sample/HelloScene.json")
            );
            world.Frame(0.0);
        }

        void OnUpdateScene(f64 seconds) override { world.Frame(seconds); }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "HelloScene",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<HelloScene>(argc, argv, windowConfig);
}
