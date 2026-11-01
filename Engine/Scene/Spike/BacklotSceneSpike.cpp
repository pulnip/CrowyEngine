#include <memory>

#include "ProjectRoot.hpp"
#include "RenderApp.hpp"
#include "Scene.hpp"
#include "SnapshotCamera.hpp"
#include "World.hpp"

namespace Crowy
{
    // Backlot's film set as ImportBacklot wrote it: nothing here knows
    // Backlot, and the checkout it came from need not exist
    class BacklotSceneSpike: public RenderApp {
        World world;

    public:
        BacklotSceneSpike()
            : RenderApp(
                  Config{
                      // the fit spans the whole set and its 128 m grid
                      .shadowMapSize = 4096,
                      .vertexPoolCapacity = 1u << 18,
                      .indexPoolCapacity = 1u << 19
                  },
                  std::make_unique<SnapshotCamera>()
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool&) override {}

        void ExtractScene(RenderScene& scene) override {
            world.Provide(scene);
            world.Provide(Meshes());
            world.Provide(Materials());
            world.Provide(static_cast<SnapshotCamera&>(Camera()).Snapshot());

            loadSceneFile(
                world,
                resolveProjectPath("Content/Backlot/Backlot.json")
            );
            world.Frame(0.0);
        }

        void OnUpdateScene(f64 seconds) override { world.Frame(seconds); }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .resizable = true,
    };
    return Main<BacklotSceneSpike>(argc, argv, windowConfig);
}
