#include <memory>

#include "GameSession.hpp"
#include "RenderApp.hpp"
#include "SnapshotCamera.hpp"

namespace Crowy
{
    // The scene file named on the command line, seen through the scene's
    // start camera. It has no camera of its own and hears no input.
    class Game: public RenderApp {
    public:
        static constexpr bool OpensScene = true;

    private:
        RAII<GameSession> session;

    public:
        Game()
            : RenderApp(
                  Config{
                      // the fit spans a whole set and its 128 m grid
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
            session->Load(*Runtime().scene);
            showStartCamera();
        }

        void OnUpdateScene(f64 seconds) override {
            session->Frame(seconds);
            showStartCamera();
        }

    private:
        void showStartCamera() {
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
    return Main<Game>(argc, argv, windowConfig);
}
