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

    // Backlot's film set in the standard pipeline, with the editor's cuts
    // and lighting keys; loaded before RenderApp exists, since the geometry
    // pool is sized from it.
    class StageEditor final: public RenderApp {
    private:
        static constexpr std::array CutKeys{
            KeyCode::Num1, KeyCode::Num2, KeyCode::Num3, KeyCode::Num4,
            KeyCode::Num5, KeyCode::Num6, KeyCode::Num7, KeyCode::Num8,
        };
        static constexpr std::array LightingKeys{KeyCode::F1, KeyCode::F2, KeyCode::F3, KeyCode::F4};
        static constexpr auto PanelKey = KeyCode::P;
        // its own file, so no other sample's windows leak in or out
        static constexpr CStr ImGuiSettings = "StageEditor.imgui.ini";

        LoadedStage stage;
        StageGeometry geometry;
        StageTextureHandles textures;
        StageBindings bindings;
        std::optional<StageContent> content;
        RAII<UIRenderer> uiRenderer;
        UIContext uiContext;
        HierarchyPanel hierarchy;
        InspectorPanel inspector;
        // last: it is exposed to the port, so it goes first
        std::optional<EditorSession> session;

    public:
        ~StageEditor() override {
            if(auto* port = Port()) {
                port->Unexpose("editor");
                port->Unexpose("camera");
            }
        }
        CROWY_DECLARE_PINNED(StageEditor)

        StageEditor()
            : StageEditor(loadStage(backlotRoot())) {}

        // the base reads `loaded` before the member takes it
        explicit StageEditor(LoadedStage loaded)
            : RenderApp(
                  makeConfig(loaded),
                  std::make_unique<EditorCamera>(editorCutOf(makeStageCuts(loaded).front()))
              ),
              stage(std::move(loaded)) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            geometry = addStageGeometry(pool, stage);
        }

        void ExtractScene(RenderScene& scene) override {
            textures = uploadStageTextures(scene, Device(), stage);
            bindings = populateStage(scene, stage, geometry, textures);
            content.emplace(scene, stage, bindings);
            session.emplace(camera(), *content, scene, [this](Color color) { SetClearColor(color); }, editorPort());
            session->Start(content->Cuts().front().name, stage.document.defaultKey);
            hierarchy.Reset(content->Objects());

            if(auto* port = Port()) {
                port->Expose("editor", &session->State(), *GetDesc<EditorState>(), [this] {
                    session->Sync();
                });
                port->Expose("camera", &camera(), *GetDesc<EditorCamera>(), [this] {
                    camera().RecomputeView();
                });
            }
        }

        void OnProcessInput(const InputProvider& input) override {
            if(input.IsKeyPressed(PanelKey))
                Debug().showPanel = !Debug().showPanel;
            if(input.IsKeyPressed(KeyCode::Escape))
                session->Select(std::nullopt);
            // a press ImGui wanted never arrives here; a look is not a pick
            if(input.IsKeyPressed(MouseButton::LButton) && !input.IsKeyDown(MouseButton::RButton))
                session->PickAt(input.GetMousePos());
            for(usize i = 0; i < CutKeys.size(); ++i) {
                if(input.IsKeyPressed(CutKeys[i]))
                    session->SelectCut(i);
            }
            for(usize i = 0; i < LightingKeys.size(); ++i) {
                if(input.IsKeyPressed(LightingKeys[i]))
                    session->SelectKey(i);
            }
        }

        void OnUpdateFrameData() override {
            const auto& io = ImGui::GetIO();
            session->Update(Vec2{io.DisplaySize.x, io.DisplaySize.y});
        }

        void OnInitUI(RHIDevice& device, const OverlayFormats& formats) override {
            uiRenderer = std::make_unique<UIRenderer>(device, formats.color, formats.depth);
            ImGui::GetIO().IniFilename = ImGuiSettings;
        }

        std::span<const RHITextureBarrier> OnPrepareUI(RHICommandList& cmdList) override {
            // the chrome stays out of every capture unless asked for
            if(Debug().showPanel) {
                if(auto* port = Port())
                    drawPortStatusChip(port->Status());
                drawEditorToolbar(*session);
                hierarchy.Draw(*session);
                inspector.Draw(*session, uiContext);
                drawSelectionHighlight(*session, Scene());
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
