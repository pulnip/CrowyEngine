#include <memory>

#include "ProjectRoot.hpp"
#include "RenderApp.hpp"
#include "Scene.hpp"
#include "SnapshotCamera.hpp"
#include "World.hpp"

namespace Crowy
{
    // a scene file of built-in meshes and materials, drawn from its camera
    // actor; nothing in it is made by code
    class HelloScene: public RenderApp {
        static constexpr Color SkyColor{0.139f, 0.212f, 0.356f, 1.0f};

        World world;

    public:
        HelloScene()
            : RenderApp(
                  Config{.clearColor = SkyColor},
                  std::make_unique<SnapshotCamera>()
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool&) override {}

        void ExtractScene(RenderScene& scene) override {
            world.Provide(scene);
            world.Provide(Meshes());
            world.Provide(Materials());
            world.Provide(static_cast<SnapshotCamera&>(Camera()).Snapshot());
            // Playground's ambient, until the environment is an actor
            scene.Environment() = EnvironmentSnapshot{
                .skyAmbient = {0.123f, 0.174f, 0.262f},
                .groundAmbient = {0.033f, 0.024f, 0.016f}
            };

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
