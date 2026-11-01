#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ClassRegistry.hpp"
#include "FrameHost.hpp"
#include "Log.hpp"
#include "Main.hpp"
#include "PortStatusChip.hpp"
#include "PropertyWalker.hpp"
#include "SceneViewEditor.hpp"
#include "StatsOverlay.hpp"
#include "UIRenderer.hpp"
#include "ViewCamera.hpp"
#include "WorldSession.hpp"

namespace Crowy
{
    struct UIContext {
        // a value changed behind the panel's widgets, which seed only when
        // they are built
        bool panelDirty = false;
    };

    // The scene file named on the command line, edited in memory through a
    // Scene View. The World runs no gameplay system.
    class Editor: public FrameHost {
    public:
        static constexpr bool OpensScene = true;

    private:
        static constexpr auto PanelToggleKey = KeyCode::P;
        static constexpr auto StatsToggleKey = KeyCode::I;

        RAII<WorldSession> session;
        // before the panel, whose rotation fields point into it
        std::optional<SceneViewEditor> sceneView;
        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        Widget panel = Column({});
        // what the panel's debug section was built from
        RenderDebug shownDebug;
        StatsOverlay statsOverlay;

    public:
        Editor()
            : FrameHost(
                  Config{
                      // the fit spans a whole set and its 128 m grid
                      .shadowMapSize = 4096,
                      .vertexPoolCapacity = 1u << 18,
                      .indexPoolCapacity = 1u << 19,
                      .showsSceneView = true
                  },
                  std::make_unique<ViewCamera>()
              ) {
            Debug().showPanel = true;
            Debug().sceneView = true;
        }

    private:
        RenderSession& openSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        ) override {
            session = std::make_unique<WorldSession>(
                std::make_unique<RenderSession>(device, pool, roots),
                SystemRegistry::Global().WithoutGameplay()
            );

            return session->Render();
        }

        void buildGeometry(GeometryPool&) override {}

        void extractScene(RenderScene& scene) override {
            session->Load(*Runtime().scene);
            showStartCamera();

            sceneView.emplace(
                session->GetWorld(),
                scene,
                editorCutOf(session->Camera()),
                Port()
            );
            shownDebug = Debug();
            panel = buildPanel();
        }

        void pollInput(const InputProvider& input) override {
            auto& debug = Debug();
            if(input.IsKeyPressed(PanelToggleKey))
                debug.showPanel = !debug.showPanel;
            if(input.IsKeyPressed(StatsToggleKey))
                debug.showStats = !debug.showStats;

            // the cursor hides while it turns, so a look never meets a screen
            // edge
            if(sceneView->PollInput(input, SceneViewID() != 0))
                RequestRelativeMouse();
        }

        void updateScene(f64 seconds) override {
            sceneView->Update(
                seconds,
                static_cast<ViewCamera&>(Camera()),
                SceneViewID() != 0
            );
            session->Frame(seconds);
            showStartCamera();
        }

        void initUI(RHIDevice& device, const OverlayFormats& formats) override {
            uiRenderer = std::make_unique<UIRenderer>(
                device,
                formats.color,
                formats.depth
            );

            LOG_INFO(
                "Editor",
                "P toggles the inspector panel, I the frame stats"
            );
        }

        std::span<const RHITextureBarrier> prepareUI(
            RHICommandList& cmdList
        ) override {
            // their own windows, closed before the SetNextWindow* below land
            if(auto* port = Port())
                drawPortStatusChip(port->Status());
            const auto& debug = Debug();
            if(debug.showStats)
                statsOverlay.Draw(LastFrameStats());

            // the port, the keys or the panel itself may have moved it
            if(debug != shownDebug) {
                shownDebug = debug;
                uiContext.panelDirty = true;
            }
            // here, before Prepare: a callback runs inside the tree's submit
            // and may only raise the flag
            if(std::exchange(uiContext.panelDirty, false))
                panel = buildPanel();

            // the running list's, not debug's, which the panel may flip
            // mid-frame
            if(const auto size = sceneView->DrawWindow(SceneViewID()))
                SetSceneViewSize(size->x, size->y);
            if(sceneView->UpdateOverlay())
                uiContext.panelDirty = true;

            if(debug.showPanel) {
                // Prepare opens the shared "Crowy" window, whose saved rect
                // may have been left collapsed or off-screen
                ImGui::SetNextWindowPos(
                    ImVec2(8.0f, 8.0f),
                    ImGuiCond_Appearing
                );
                ImGui::SetNextWindowSize(
                    ImVec2(360.0f, 640.0f),
                    ImGuiCond_Appearing
                );
                ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);

                uiRenderer->Prepare(cmdList, panel, uiContext);
            }
            else {
                uiRenderer->Prepare(cmdList);
            }

            return uiRenderer->TextureAcquires();
        }

        void recordUI(RHICommandList& cmdList) override {
            uiRenderer->Record(cmdList);
        }

        void showStartCamera() {
            static_cast<ViewCamera&>(Camera()).Snapshot() = session->Camera();
        }

        Widget buildPanel() {
            // record reads debug every frame; no dirty consumer
            std::vector<Widget> sections{buildPropertyTree(
                "debug",
                &Debug(),
                *getDesc<RenderDebug>(),
                [] {}
            )};
            const DirtyCallback refresh = [this] {
                uiContext.panelDirty = true;
            };
            if(auto selection = sceneView->SelectionSection(refresh))
                sections.push_back(std::move(*selection));

            return Column(std::move(sections));
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
    return Main<Editor>(argc, argv, windowConfig);
}
