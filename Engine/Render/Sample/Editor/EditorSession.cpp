#include "EditorSession.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "Assert.hpp"
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
        const bool& chromeShown,
        ClearColorSink clearColor,
        EditorPort port
    )
        : camera(camera),
          content(content),
          scene(scene),
          chromeShown(chromeShown),
          clearColor(std::move(clearColor)),
          port(std::move(port)) {
        exposeTargets();
    }

    EditorSession::~EditorSession() {
        unexposeSelection();
        if(port.unexpose) {
            port.unexpose(GizmoTarget);
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
        if(state.cancel)
            applyCancel();
        if(state.handle != applied.handle)
            applyHandle();
        if(state.grab != EditorNoPick)
            applyGrab();
        if(state.drag != EditorNoPick)
            applyDrag();
        refreshGizmo();
        applied = state;
    }

    void EditorSession::Update(Vec2 windowSize) {
        SetViewport(windowSize);
        if(camera.TakeMoved())
            leaveCut();
        refreshGizmo();
    }

    void EditorSession::SetViewport(Vec2 windowSize) {
        if(windowSize.x > 0.0f && windowSize.y > 0.0f)
            viewport = windowSize;
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

        release();
        unexposeSelection();
        selection = object;
        state.selected = object ? objects[*object].name : Str{};
        applied.selected = state.selected;
        inspected = object ? content.Inspect(*object) : InspectSections{};
        for(auto& section: inspected) {
            if(!section.apply)
                section.apply = [] {};
        }
        exposeSelection();
        bindGizmo();
        refreshGizmo();
        selectionChanged = true;
        reportSelection();
    }

    void EditorSession::PickAt(Vec2 pixel) {
        state.pickAt = pixel;
        Sync();
    }

    bool EditorSession::Grab(Vec2 pixel) {
        state.grab = pixel;
        Sync();

        return hold.has_value();
    }

    void EditorSession::DragTo(Vec2 pixel, bool snap) {
        state.snap = snap;
        state.drag = pixel;
        Sync();
    }

    void EditorSession::Release() {
        state.handle = GizmoHandle::None;
        Sync();
    }

    void EditorSession::Cancel() {
        state.cancel = true;
        Sync();
    }

    std::optional<GizmoLayout> EditorSession::SelectionGizmo() const {
        if(!chromeShown || !gizmoRow)
            return std::nullopt;

        const auto yaw = gizmoRow->yaw ? *gizmoRow->yaw : 0.0f;

        return layoutGizmo(camera, viewport, *gizmoRow->position, yaw, gizmoRow->parts);
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
            refreshGizmo();
        });
        // read-only: a write is put back
        port.expose(GizmoTarget, &gizmoView, *GetDesc<GizmoView>(), [this] { refreshGizmo(); });
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
        // a key rewrites rows the inspector may show
        inspectorDirty = true;
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
        const auto pixel = std::exchange(state.pickAt, EditorNoPick);
        if(const auto light = markerAt(pixel)) {
            Select(light);
            return;
        }

        std::optional<PickHit> hit;
        if(const auto ray = rayThroughPixel(camera, pixel, viewport)) {
            hit = pickScene(scene, *ray, [this](PrimitiveHandle primitive) {
                return content.MeshesOf(primitive);
            });
        }
        Select(hit ? content.ObjectOf(hit->primitive) : std::nullopt);
    }

    void EditorSession::applyGrab() {
        const auto pixel = std::exchange(state.grab, EditorNoPick);
        release();
        const auto layout = SelectionGizmo();
        if(!layout) {
            refuse(chromeShown ? "nothing selected carries a gizmo in view" : "the gizmo shows with debug.showPanel");
            return;
        }

        const auto handle = hoverGizmo(projectGizmo(*layout), pixel);
        const auto ray = rayThroughPixel(camera, pixel, viewport);
        const auto drag = handle != GizmoHandle::None && ray ? grabGizmo(*layout, handle, *ray) : std::nullopt;
        if(!drag) {
            refuse(std::format("no handle at {}, {}", pixel.x, pixel.y));
            return;
        }

        const auto& row = *gizmoRow;
        hold = GizmoHold{
            .drag = *drag,
            .position = *row.position,
            .yaw = row.yaw ? *row.yaw : 0.0f,
            .scale = row.scale ? *row.scale : ones()
        };
        state.handle = handle;
        refuse(std::format("holding {}", enumName(handle)));
    }

    void EditorSession::applyDrag() {
        const auto pixel = std::exchange(state.drag, EditorNoPick);
        if(!hold) {
            refuse("no handle is held; write editor.grab first");
            return;
        }

        const auto ray = rayThroughPixel(camera, pixel, viewport);
        const auto edit = ray ? dragGizmo(hold->drag, *ray, state.snap) : std::nullopt;
        // a ray that misses the plane leaves the last change standing
        if(!edit)
            return;

        auto& row = *gizmoRow;
        const auto handle = hold->drag.handle;
        if(isGizmoArrow(handle))
            *row.position = hold->position + edit->offset;
        else if(handle == GizmoHandle::Ring)
            *row.yaw = wrapDegrees(hold->yaw + edit->turn);
        else
            *row.scale = hold->scale * edit->factor;
        wroteSection(0);
    }

    void EditorSession::applyHandle() {
        if(state.handle == GizmoHandle::None) {
            release();
            return;
        }
        // only a press takes a handle
        state.handle = Held();
        refuse("editor.handle names the held handle: write editor.grab to take one, None to let go");
    }

    void EditorSession::applyCancel() {
        state.cancel = false;
        if(!hold)
            return;

        auto& row = *gizmoRow;
        *row.position = hold->position;
        if(row.yaw)
            *row.yaw = hold->yaw;
        if(row.scale)
            *row.scale = hold->scale;
        release();
        wroteSection(0);
    }

    void EditorSession::release() {
        hold.reset();
        state.handle = GizmoHandle::None;
    }

    void EditorSession::bindGizmo() {
        gizmoRow.reset();
        if(inspected.empty() || inspected.front().gizmo == GizmoParts::None)
            return;

        const auto& section = inspected.front();
        // the accessor a port write goes through, checked for the type it must be
        const auto field = [&](CStr name, const TypeOps* type) -> void* {
            const auto resolved = ResolveProperty(section.target, *section.desc, name);
            return resolved.desc != nullptr && &resolved.desc->type == type ? resolved.member : nullptr;
        };
        GizmoRow row{
            .position = static_cast<Vec3*>(field("position", GetTypeOps<Vec3>())),
            .yaw = static_cast<f32*>(field("yaw", GetTypeOps<f32>())),
            .scale = static_cast<Vec3*>(field("scale", GetTypeOps<Vec3>())),
            .parts = section.gizmo
        };
        CROWY_ASSERT(row.position != nullptr, "a section offering a gizmo has a Vec3 position");
        CROWY_ASSERT(row.yaw != nullptr || !hasFlag(row.parts, GizmoParts::Turn), "the ring needs an f32 yaw");
        CROWY_ASSERT(
            row.scale != nullptr || !(hasFlag(row.parts, GizmoParts::Scale) || hasFlag(row.parts, GizmoParts::ScaleAxes)),
            "a scale handle needs a Vec3 scale"
        );
        if(row.position != nullptr)
            gizmoRow = row;
    }

    void EditorSession::refreshGizmo() {
        if(hold && !chromeShown)
            release();

        gizmoView = GizmoView{};
        const auto layout = SelectionGizmo();
        if(!layout)
            return;

        using enum GizmoHandle;
        const auto aim = [&](GizmoHandle handle) { return aimGizmo(*layout, handle).value_or(GizmoAim{}); };
        gizmoView.pivot = projectToWindow(layout->viewProj, layout->pivot, layout->viewport).value_or(EditorNoPick);
        gizmoView.length = layout->length;
        gizmoView.moveX = aim(MoveX);
        gizmoView.moveY = aim(MoveY);
        gizmoView.moveZ = aim(MoveZ);
        gizmoView.ring = aim(Ring);
        gizmoView.scale = aim(Scale);
        gizmoView.scaleX = aim(ScaleX);
        gizmoView.scaleY = aim(ScaleY);
        gizmoView.scaleZ = aim(ScaleZ);
    }

    void EditorSession::wroteSection(usize section) {
        inspected[section].apply();
        inspectorDirty = true;
        refreshGizmo();
    }

    // the light whose marker is drawn nearest the pixel
    std::optional<usize> EditorSession::markerAt(Vec2 pixel) const {
        std::vector<usize> objects;
        std::vector<Vec3> markers;
        for(usize i = 0; i < content.Objects().size(); ++i) {
            if(const auto light = content.LightOf(i); light && scene.Lights().IsValid(*light)) {
                objects.push_back(i);
                markers.push_back(scene.Lights().GetRef(*light).position);
            }
        }
        const auto viewProj = camera.ViewProj(viewport.x / viewport.y);
        const auto nearest = nearestOnScreen(viewProj, viewport, pixel, markers, MarkerRadius);

        return nearest ? std::optional(objects[*nearest]) : std::nullopt;
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
            port.expose(name, section.target, *section.desc, [this, i] { wroteSection(i); });
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
        .SetProperty("grab", &EditorState::grab)
        .SetProperty("drag", &EditorState::drag)
        .SetProperty("handle", &EditorState::handle)
        .SetProperty("snap", &EditorState::snap)
        .SetProperty("cancel", &EditorState::cancel)
        .SetProperty("status", &EditorState::status)
    CROWY_STRUCT_END(EditorState)

    CROWY_STRUCT(GizmoAim)
        .SetProperty("grab", &GizmoAim::grab)
        .SetProperty("reach", &GizmoAim::reach)
    CROWY_STRUCT_END(GizmoAim)

    CROWY_STRUCT(GizmoView)
        .SetProperty("pivot", &GizmoView::pivot)
        .SetProperty("length", &GizmoView::length)
        .SetProperty("moveX", &GizmoView::moveX)
        .SetProperty("moveY", &GizmoView::moveY)
        .SetProperty("moveZ", &GizmoView::moveZ)
        .SetProperty("ring", &GizmoView::ring)
        .SetProperty("scale", &GizmoView::scale)
        .SetProperty("scaleX", &GizmoView::scaleX)
        .SetProperty("scaleY", &GizmoView::scaleY)
        .SetProperty("scaleZ", &GizmoView::scaleZ)
    CROWY_STRUCT_END(GizmoView)

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
