#include "SceneViewEditor.hpp"

#include <algorithm>
#include <format>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ClassRegistry.hpp"
#include "WorldEditorOverlay.hpp"

namespace Crowy
{
    namespace
    {
        // the scene as a button the size of the image, so a drag on it turns
        // the camera instead of moving the window; nullopt with no room for it
        std::optional<SceneImage> drawSceneView(u64 sceneView, ImVec2 size) {
            constexpr auto Buttons = ImGuiButtonFlags_MouseButtonLeft |
                                     ImGuiButtonFlags_MouseButtonRight |
                                     ImGuiButtonFlags_MouseButtonMiddle;

            // a zero side is a collapsed window, which the button asserts on
            if(size.x <= 0.0f || size.y <= 0.0f)
                return std::nullopt;

            const auto origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("scene", size, Buttons);
            // the scene's input is the scene's: the mouse over it, the keys
            // too while a press on it holds the button
            if(ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetNextFrameWantCaptureMouse(false);
            if(ImGui::IsItemActive())
                ImGui::SetNextFrameWantCaptureKeyboard(false);

            ImGui::GetWindowDrawList()->AddImage(
                static_cast<ImTextureID>(sceneView),
                origin,
                ImVec2(origin.x + size.x, origin.y + size.y)
            );

            return SceneImage{
                .origin = {origin.x, origin.y},
                .size = {size.x, size.y}
            };
        }
    }

    SceneViewEditor::SceneViewEditor(
        World& world,
        RenderScene& scene,
        const EditorCut& cut,
        CommandPort* port
    )
        : world(world),
          scene(scene),
          camera(cut),
          editor(world, scene, camera, port),
          cameraExposure(port) {
        cameraExposure.Expose(
            "sceneCamera",
            &camera,
            *getDesc<EditorCamera>(),
            [this] { camera.RecomputeView(); }
        );
    }

    bool SceneViewEditor::PollInput(const InputProvider& input, bool viewRuns) {
        if(input.IsKeyPressed(KeyCode::Escape)) {
            if(editor.Held() != GizmoHandle::None)
                editor.Cancel();
            else
                editor.Select(std::nullopt);
        }
        // a press ImGui wanted never arrives here; a look is not a press. A
        // press on a handle takes it, elsewhere it picks
        if(image) {
            const auto point = input.GetMousePos() - image->origin;
            if(input.IsKeyPressed(MouseButton::LButton) &&
               !input.IsKeyDown(MouseButton::RButton)) {
                mouseHolds = editor.Grab(point);
                if(!mouseHolds)
                    editor.PickAt(point);
            }
            else if(mouseHolds && editor.Held() == GizmoHandle::None) {
                // the port or Esc let go already
                mouseHolds = false;
            }
            else if(mouseHolds && input.IsKeyDown(MouseButton::LButton)) {
                editor.DragTo(point, input.IsKeyDown(KeyCode::Ctrl));
            }
            else if(mouseHolds) {
                editor.Release();
                mouseHolds = false;
            }
        }

        if(!viewRuns)
            return false;

        camera.PollInput(input);
        return input.IsKeyDown(MouseButton::RButton);
    }

    void SceneViewEditor::Update(f64 seconds, ViewCamera& view, bool viewRuns) {
        view.Show(viewRuns ? &camera : nullptr);
        if(viewRuns)
            camera.Update(seconds);
    }

    std::optional<Size2D> SceneViewEditor::DrawWindow(u64 sceneView) {
        image.reset();
        if(sceneView == 0)
            return std::nullopt;

        std::optional<Size2D> content;
        // the image fills the window, so the window cannot size itself; right
        // of where the panel opens, so a run without a saved layout has the
        // two apart
        ImGui::SetNextWindowPos(ImVec2(376.0f, 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(640.0f, 400.0f),
            ImGuiCond_FirstUseEver
        );
        if(ImGui::Begin("Scene View")) {
            const auto size = ImGui::GetContentRegionAvail();
            content = Size2D{
                static_cast<u32>(std::max(size.x, 0.0f)),
                static_cast<u32>(std::max(size.y, 0.0f))
            };
            image = drawSceneView(sceneView, size);
        }
        ImGui::End();

        return content;
    }

    bool SceneViewEditor::UpdateOverlay() {
        editor.Update(image ? std::optional(image->size) : std::nullopt);
        const auto changed =
            editor.TakeInspectorDirty() | editor.TakeSelectionChanged();

        if(image) {
            drawWorldEditorOverlay(
                editor,
                world,
                scene,
                camera,
                image->origin,
                image->size
            );
        }

        return changed;
    }

    std::optional<Widget> SceneViewEditor::SelectionSection(
        const DirtyCallback& onChange
    ) {
        const auto selection = editor.Selection();
        if(!selection)
            return std::nullopt;

        const auto overrides = eulerInspector.Overrides();
        std::vector<Widget> selected;
        for(const auto& section: editor.Sections()) {
            selected.push_back(buildPropertyTree(
                section.label.c_str(),
                section.target,
                onChange,
                overrides
            ));
        }

        return Collapsing{
            .label =
                std::format("selection · {}", editor.DisplayName(*selection)),
            .children = std::move(selected),
            .defaultOpen = true
        };
    }
}
