#include <memory>

#include "GameSession.hpp"
#include "ProjectRoot.hpp"
#include "RenderApp.hpp"
#include "SnapshotCamera.hpp"

namespace Crowy
{
    // Backlot's film set as ImportBacklot wrote it: nothing here knows
    // Backlot, and the checkout it came from need not exist
    class BacklotSceneSpike: public RenderApp {
        RAII<GameSession> session;

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
        RenderSession& OnOpenSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        ) override {
            session = std::make_unique<GameSession>(device, pool, roots);

            return session->Render();
        }

        void OnBuildGeometry(GeometryPool&) override {}

        void ExtractScene(RenderScene&) override {
            session->Load(resolveProjectPath("Content/Backlot/Backlot.json"));
            showSceneCamera();
        }

        void OnUpdateScene(f64 seconds) override {
            session->Frame(seconds);
            showSceneCamera();
        }

    private:
        void showSceneCamera() {
            static_cast<SnapshotCamera&>(Camera()).Snapshot() =
                session->Camera();
        }
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
