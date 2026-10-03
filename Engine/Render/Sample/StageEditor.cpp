#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>

#include <imgui.h>

#include "ClassRegistry.hpp"
#include "EditorCamera.hpp"
#include "EditorPanels.hpp"
#include "EditorSession.hpp"
#include "InputProvider.hpp"
#include "PortStatusChip.hpp"
#include "RenderApp.hpp"
#include "StageContent.hpp"
#include "StageScene.hpp"
#include "StringUtil.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    // each executable defines the UI context its widgets share; the editor
    // draws raw ImGui and shares nothing
    struct UIContext {};

    namespace
    {
        std::filesystem::path backlotRoot() {
            return toPath(CROWY_BACKLOT_DIR);
        }

        // what the stage needs on disk; empty when it is all there
        Str missingContent() {
            for(const auto* relative: {StageScenePath, "Unity/Assets/Art/Textures/Palette.png"}) {
                const auto file = backlotRoot() / relative;
                if(!std::filesystem::exists(file) || isLfsPointer(file))
                    return toUTF8String(file);
            }

            return {};
        }
    }

    // Backlot's film set under the editor; loaded before RenderApp exists,
    // since the geometry pool is sized from it.
    class StageEditor final: public RenderApp {
    private:
        LoadedStage stage;
        StageGeometry geometry;
        StageTextureHandles textures;
        std::optional<StageContent> content;
        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        HierarchyPanel hierarchy;
        InspectorPanel inspector;
        // a hold the mouse took, which only the mouse drags and lets go
        bool mouseHolds = false;
        // last: it leaves the port first
        std::optional<EditorSession> session;

    public:
        StageEditor()
            : StageEditor(loadStage(backlotRoot())) {}

        // the base reads `stage` before the member takes it
        explicit StageEditor(LoadedStage stage)
            : RenderApp(
                  makeConfig(stage),
                  std::make_unique<EditorCamera>(editorCutOf(makeStageCuts(stage).front()))
              ),
              stage(std::move(stage)) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            geometry = addStageGeometry(pool, stage);
        }

        void ExtractScene(RenderScene& scene) override {
            textures = uploadStageTextures(scene, Device(), stage);
            content.emplace(scene, stage, geometry, textures);
            session.emplace(
                camera(),
                *content,
                scene,
                Debug().showPanel,
                [this](Color color) { SetClearColor(color); },
                editorPort()
            );
            const auto file = (backlotRoot() / StageScenePath).lexically_normal();
            session->Start(content->Cuts().front().name, stage.document.defaultKey, toUTF8String(file));
            // a port pick may come before the first frame measures the window
            const auto& window = Runtime().window;
            session->SetViewport(Vec2{static_cast<f32>(window.width), static_cast<f32>(window.height)});
        }

        void OnProcessInput(const InputProvider& input) override {
            constexpr std::array CutKeys{
                KeyCode::Num1, KeyCode::Num2, KeyCode::Num3, KeyCode::Num4,
                KeyCode::Num5, KeyCode::Num6, KeyCode::Num7, KeyCode::Num8,
            };
            constexpr std::array LightingKeys{KeyCode::F1, KeyCode::F2, KeyCode::F3, KeyCode::F4};

            if(input.IsKeyPressed(KeyCode::P))
                Debug().showPanel = !Debug().showPanel;
            if(input.IsKeyPressed(KeyCode::F5))
                session->Reload();
            // Esc undoes a held drag, else lets go of the selection
            if(input.IsKeyPressed(KeyCode::Escape)) {
                if(session->Held() != GizmoHandle::None) {
                    session->Cancel();
                } else {
                    session->State().selected.clear();
                    session->Sync();
                }
            }
            // a press ImGui wanted never arrives here; a look is not a press. A
            // press on a handle takes it, elsewhere it picks
            const auto mouse = input.GetMousePos();
            if(input.IsKeyPressed(MouseButton::LButton) && !input.IsKeyDown(MouseButton::RButton)) {
                mouseHolds = session->Grab(mouse);
                if(!mouseHolds)
                    session->PickAt(mouse);
            } else if(mouseHolds) {
                // the port, Esc or another selection may have let go already
                if(session->Held() == GizmoHandle::None) {
                    mouseHolds = false;
                } else if(input.IsKeyDown(MouseButton::LButton)) {
                    session->DragTo(mouse, input.IsKeyDown(KeyCode::Ctrl));
                } else {
                    session->Release();
                    mouseHolds = false;
                }
            }
            for(usize i = 0; i < CutKeys.size(); ++i) {
                if(input.IsKeyPressed(CutKeys[i]))
                    session->SelectCut(i);
            }
            for(usize i = 0; i < LightingKeys.size(); ++i) {
                if(input.IsKeyPressed(LightingKeys[i]))
                    session->SelectKey(i);
            }
        }

        void OnUpdateScene(f64 seconds) override {
            session->Advance(seconds);
        }

        void OnUpdateFrameData() override {
            const auto& io = ImGui::GetIO();
            session->Update(Vec2{io.DisplaySize.x, io.DisplaySize.y});
            // a text field has the keyboard: typing must not fly the camera
            camera().keyboardGated = io.WantCaptureKeyboard;
        }

        void OnInitUI(RHIDevice& device, const OverlayFormats& formats) override {
            // its own file, so no other sample's windows leak in or out
            constexpr CStr ImGuiSettings = "StageEditor.imgui.ini";

            uiRenderer = std::make_unique<UIRenderer>(device, formats.color, formats.depth);
            ImGui::GetIO().IniFilename = ImGuiSettings;
        }

        std::span<const RHITextureBarrier> OnPrepareUI(RHICommandList& cmdList) override {
            constexpr CStr ToolbarHint =
                "1-8 cuts, F1-F4 keys, click to select, drag a handle (left Ctrl snaps), Esc undoes or clears, F5 reloads, P hides";

            // the chrome stays out of every capture unless asked for
            if(Debug().showPanel) {
                if(auto* port = Port())
                    drawPortStatusChip(port->Status());
                drawEditorToolbar(*session, ToolbarHint);
                hierarchy.Draw(*session);
                inspector.Draw(*session, uiContext);
                drawLightMarkers(*session, Scene());
                drawSelectionHighlight(*session, Scene());
                drawGizmo(*session);
            }
            uiRenderer->Prepare(cmdList);

            return uiRenderer->TextureAcquires();
        }

        void OnRecordUI(RHICommandList& cmdList) override {
            uiRenderer->Record(cmdList);
        }

    private:
        static Config makeConfig(const LoadedStage& loaded) {
            const auto& document = loaded.document;
            const auto& key = document.lightingKeys[stageKeyIndex(document, document.defaultKey)];
            const auto& capacities = loaded.capacities;

            return Config{
                .clearColor = {key.skyHorizon.x, key.skyHorizon.y, key.skyHorizon.z, 1.0f},
                .drawCapacity = capacities.draws,
                .materialCapacity = capacities.materials,
                // the fit spans the whole set and its 128 m grid
                .shadowMapSize = 4096,
                .vertexPoolCapacity = capacities.vertices,
                .indexPoolCapacity = capacities.indices
            };
        }

        EditorPort editorPort() {
            auto* port = Port();
            if(port == nullptr)
                return {};

            return EditorPort{
                .expose = [port](StrView name, void* target, const TypeDesc& desc, DirtyCallback onDirty) {
                    port->Expose(Str(name), target, desc, std::move(onDirty));
                },
                .unexpose = [port](StrView name) { port->Unexpose(name); }
            };
        }

        EditorCamera& camera() noexcept {
            return static_cast<EditorCamera&>(Camera());
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    // the content is another repository: without it there is nothing to show
    if(const auto missing = missingContent(); !missing.empty()) {
        std::printf(
            "skipped: %s is missing or a Git LFS pointer (clone Backlot beside "
            "this repository and git lfs pull, or set CROWY_BACKLOT_DIR)\n",
            missing.c_str()
        );
        return 77;
    }

    const WindowConfig windowConfig{
        .title = "StageEditor",
        .width = 1920,
        .height = 1080,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<StageEditor>(argc, argv, windowConfig);
}
