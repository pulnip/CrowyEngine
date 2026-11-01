#include <memory>

#include "FrameHost.hpp"
#include "GameSession.hpp"
#include "Main.hpp"
#include "SnapshotCamera.hpp"

namespace Crowy
{
    // The scene file named on the command line, seen through the scene's
    // start camera. It has no camera of its own and hears no input.
    class Game: public FrameHost {
    public:
        static constexpr bool OpensScene = true;

    private:
        RAII<GameSession> session;

    public:
        Game()
            : FrameHost(
                  Config{
                      // the fit spans a whole set and its 128 m grid
                      .shadowMapSize = 4096,
                      .vertexPoolCapacity = 1u << 18,
                      .indexPoolCapacity = 1u << 19
                  },
                  std::make_unique<SnapshotCamera>()
              ) {}

    private:
        RenderSession& openSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        ) override {
            session = std::make_unique<GameSession>(
                std::make_unique<RenderSession>(device, pool, roots)
            );

            return session->Render();
        }

        void buildGeometry(GeometryPool&) override {}

        void extractScene(RenderScene&) override {
            session->Load(*Runtime().scene);
            showStartCamera();
        }

        void updateScene(f64 seconds) override {
            session->Frame(seconds);
            showStartCamera();
        }

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
