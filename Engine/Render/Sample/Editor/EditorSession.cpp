#include "EditorSession.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "ClassRegistry.hpp"
#include "Object.hpp"

namespace Crowy
{
    namespace
    {
        template<typename R, typename F>
        Str listOf(const R& items, F&& name) {
            Str list;
            for(const auto& item: items)
                list += list.empty() ? Str(name(item)) : ", " + Str(name(item));

            return list;
        }

        // a full name, or a bare one that only one object carries
        std::optional<usize> objectNamed(EditorObjects objects, StrView name, Str& why) {
            std::optional<usize> found;
            for(usize i = 0; i < objects.size(); ++i) {
                const auto& full = objects[i].name;
                if(full == name)
                    return i;

                const auto slash = full.find('/');
                if(slash != Str::npos && StrView(full).substr(slash + 1) == name) {
                    if(found) {
                        why = std::format("'{}' names more than one object; write it as <kind>/{}", name, name);
                        return std::nullopt;
                    }
                    found = i;
                }
            }
            if(!found)
                why = std::format("no object '{}'", name);

            return found;
        }
    }

    EditorSession::EditorSession(
        EditorCamera& camera,
        EditorContent& content,
        const RenderScene& scene,
        ClearColorSink clearColor,
        EditorPort port
    )
        : camera(camera),
          content(content),
          scene(scene),
          clearColor(std::move(clearColor)),
          port(std::move(port)) {
        exposeTargets();
    }

    EditorSession::~EditorSession() {
        unexposeSelection();
        if(port.unexpose) {
            port.unexpose(CameraTarget);
            port.unexpose(StateTarget);
        }
    }

    void EditorSession::Start(StrView cut, StrView key) {
        state.cut = Str(cut);
        state.key = Str(key);
        if(!applyCut() || !applyKey())
            throw std::runtime_error(state.status);
        applied = state;
    }

    void EditorSession::Sync() {
        if(state.status != applied.status)
            state.status = applied.status;
        if(state.cut != applied.cut && !applyCut())
            state.cut = applied.cut;
        if(state.key != applied.key && !applyKey())
            state.key = applied.key;
        if(state.pickAt != EditorNoPick)
            applyPickAt();
        else if(state.selected != applied.selected && !applySelected())
            state.selected = applied.selected;
        applied = state;
    }

    void EditorSession::Update(Vec2 windowSize) {
        viewport = windowSize;
        if(camera.TakeMoved())
            leaveCut();
    }

    void EditorSession::SelectCut(usize index) {
        const auto cuts = content.Cuts();
        if(index >= cuts.size())
            return;

        state.cut = cuts[index].name;
        // the same cut again snaps back after flying
        applied.cut = FreeCut;
        Sync();
    }

    void EditorSession::SelectKey(usize index) {
        const auto keys = content.Keys();
        if(index >= keys.size())
            return;

        state.key = keys[index];
        Sync();
    }

    void EditorSession::Select(std::optional<usize> object) {
        const auto objects = content.Objects();
        if(object && *object >= objects.size())
            object.reset();

        unexposeSelection();
        selection = object;
        state.selected = object ? objects[*object].name : Str{};
        applied.selected = state.selected;
        inspected = object ? content.Inspect(*object) : InspectSections{};
        exposeSelection();
        selectionChanged = true;
        reportSelection();
    }

    void EditorSession::PickAt(Vec2 pixel) {
        state.pickAt = pixel;
        Sync();
    }

    bool EditorSession::TakeSelectionChanged() noexcept {
        return std::exchange(selectionChanged, false);
    }

    bool EditorSession::TakeInspectorDirty() noexcept {
        return std::exchange(inspectorDirty, false);
    }

    void EditorSession::exposeTargets() {
        if(!port.expose)
            return;

        port.expose(StateTarget, &state, *GetDesc<EditorState>(), [this] { Sync(); });
        port.expose(CameraTarget, &camera, *GetDesc<EditorCamera>(), [this] {
            camera.RecomputeView();
            leaveCut();
        });
    }

    void EditorSession::leaveCut() {
        state.cut = FreeCut;
        applied.cut = FreeCut;
    }

    bool EditorSession::applyCut() {
        if(state.cut == FreeCut)
            return true;

        const auto cuts = content.Cuts();
        const auto found = std::ranges::find(cuts, state.cut, &EditorCut::name);
        if(found == cuts.end()) {
            refuse(std::format(
                "no cut '{}'; there are {}, or {}",
                state.cut,
                listOf(cuts, [](const EditorCut& cut) { return cut.name; }),
                FreeCut
            ));
            return false;
        }

        camera.Snap(*found);
        camera.TakeMoved();
        state.status = std::format("cut {}", state.cut);
        applied.status = state.status;

        return true;
    }

    bool EditorSession::applyKey() {
        const auto keys = content.Keys();
        if(std::ranges::find(keys, state.key) == keys.end()) {
            refuse(std::format("no key '{}'; there are {}", state.key, listOf(keys, [](const Str& key) { return key; })));
            return false;
        }

        clearColor(content.ApplyKey(state.key));
        state.status = std::format("key {}", state.key);
        applied.status = state.status;

        return true;
    }

    bool EditorSession::applySelected() {
        if(state.selected.empty()) {
            Select(std::nullopt);
            return true;
        }

        Str why;
        const auto object = objectNamed(content.Objects(), state.selected, why);
        if(!object) {
            refuse(why);
            return false;
        }

        Select(object);
        return true;
    }

    void EditorSession::applyPickAt() {
        const auto hit = pickScene(
            scene,
            rayThroughPixel(camera, state.pickAt, viewport),
            [this](PrimitiveHandle primitive) { return content.MeshesOf(primitive); }
        );
        // a pulse: the same pixel written again picks again, as a click does
        state.pickAt = EditorNoPick;
        Select(hit ? content.ObjectOf(hit->primitive) : std::nullopt);
    }

    void EditorSession::refuse(Str status) {
        state.status = std::move(status);
        applied.status = state.status;
    }

    // a port write applies like a panel edit, and the panel rebuilds
    void EditorSession::exposeSelection() {
        if(!port.expose)
            return;

        for(usize i = 0; i < inspected.size(); ++i) {
            const auto& section = inspected[i];
            auto name = i == 0 ? Str(SelectionTarget) : std::format("{}.{}", SelectionTarget, section.label);
            port.expose(name, section.target, *section.desc, [this, apply = section.apply] {
                if(apply)
                    apply();
                inspectorDirty = true;
            });
            exposed.push_back(std::move(name));
        }
    }

    void EditorSession::unexposeSelection() {
        if(port.unexpose) {
            for(const auto& name: exposed)
                port.unexpose(name);
        }
        exposed.clear();
    }

    void EditorSession::reportSelection() {
        state.status = selection ? std::format("selected {}", state.selected) : Str("nothing selected");
        applied.status = state.status;
    }

    // clang-format off
    CROWY_STRUCT(EditorState)
        .SetProperty("cut", &EditorState::cut)
        .SetProperty("key", &EditorState::key)
        .SetProperty("selected", &EditorState::selected)
        .SetProperty("pickAt", &EditorState::pickAt)
        .SetProperty("status", &EditorState::status)
    CROWY_STRUCT_END(EditorState)

    CROWY_STRUCT(EditorCamera)
        .SetProperty("position", &EditorCamera::position)
        .SetProperty("yaw", &EditorCamera::yaw)
        .SetProperty("pitch", &EditorCamera::pitch)
        .SetUIRange(-1.57f, 1.57f)
        .SetProperty("fovY", &EditorCamera::lens, &EditorLens::fovY)
        .SetUIRange(0.2f, 2.4f)
        .SetProperty("orthographic", &EditorCamera::lens, &EditorLens::orthographic)
        .SetProperty("orthoHalfHeight", &EditorCamera::lens, &EditorLens::orthoHalfHeight)
        .SetProperty("nearZ", &EditorCamera::lens, &EditorLens::nearZ)
    CROWY_STRUCT_END(EditorCamera)
    // clang-format on
}
