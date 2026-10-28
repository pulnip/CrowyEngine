#include "EditorSession.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <format>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "Assert.hpp"
#include "ClassRegistry.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        struct GizmoViewAim {
            GizmoHandle handle = GizmoHandle::None;
            CStr name = nullptr;
            GizmoAim GizmoView::* aim = nullptr;
        };

        constexpr std::array<GizmoViewAim, GizmoHandleCount - 1> GizmoViewAims{{
            {GizmoHandle::MoveX, "moveX", &GizmoView::moveX},
            {GizmoHandle::MoveY, "moveY", &GizmoView::moveY},
            {GizmoHandle::MoveZ, "moveZ", &GizmoView::moveZ},
            {GizmoHandle::Ring, "ring", &GizmoView::ring},
            {GizmoHandle::Scale, "scale", &GizmoView::scale},
            {GizmoHandle::ScaleX, "scaleX", &GizmoView::scaleX},
            {GizmoHandle::ScaleY, "scaleY", &GizmoView::scaleY},
            {GizmoHandle::ScaleZ, "scaleZ", &GizmoView::scaleZ},
        }};
        static_assert(std::ranges::all_of(
            std::views::iota(usize{1}, GizmoHandleCount),
            [](usize handle) {
                return std::ranges::contains(
                    GizmoViewAims,
                    static_cast<GizmoHandle>(handle),
                    &GizmoViewAim::handle
                );
            }
        ));

        std::expected<usize, Str> objectNamed(
            EditorObjects objects,
            StrView name
        ) {
            std::optional<usize> found;
            for(usize i = 0; i < objects.size(); ++i) {
                const auto& full = objects[i].name;
                if(full == name)
                    return i;

                const auto slash = full.find('/');
                if(slash != Str::npos &&
                   StrView(full).substr(slash + 1) == name) {
                    if(found) {
                        return std::unexpected(
                            std::format(
                                "'{}' names more than one object; write it as "
                                "<kind>/{}",
                                name,
                                name
                            )
                        );
                    }
                    found = i;
                }
            }
            if(!found)
                return std::unexpected(std::format("no object '{}'", name));

            return *found;
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

    void EditorSession::Start(StrView cut, StrView key, StrView scene) {
        state.cut = Str(cut);
        state.key = Str(key);
        state.scene = Str(scene);
        loadedScene = state.scene;
        if(!applyCut() || !applyKey())
            throw std::runtime_error(state.status);
        applied = state;
    }

    void EditorSession::Sync() {
        if(state.status != applied.status)
            state.status = applied.status;
        if(state.revision != applied.revision)
            state.revision = applied.revision;
        if(applied.reload)
            state.reload = true;
        if(state.scene.empty()) {
            state.scene = applied.scene;
            Report("editor.scene names a scene file");
        }
        if(state.time != applied.time) {
            if(std::isfinite(state.time) && state.time >= 0.0) {
                showTime();
            }
            else {
                state.time = applied.time;
                Report("editor.time is scene seconds, 0 or more");
            }
        }
        if(state.cut != applied.cut && !applyCut())
            state.cut = applied.cut;
        if(state.key != applied.key && !applyKey())
            state.key = applied.key;
        if(state.pickAt != EditorNoPick)
            applyPickAt();
        else if(state.selected != applied.selected && !applySelected())
            state.selected = applied.selected;
        // hidden chrome lets go before any gizmo write can land
        if(hold && !chromeShown)
            release();
        if(state.cancel)
            applyCancel();
        if(state.handle != Held())
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

    void EditorSession::Advance(f64 seconds) {
        if(state.reload)
            reloadScene();
        if(!state.paused) {
            state.time += seconds;
            applied.time = state.time;
        }
        showTime();
    }

    void EditorSession::Reload() {
        state.reload = true;
        Sync();
    }

    void EditorSession::Report(Str status) {
        state.status = std::move(status);
        applied.status = state.status;
    }

    void EditorSession::TogglePause() {
        state.paused = !state.paused;
        Sync();
    }

    void EditorSession::StepTime(i32 frames) {
        state.paused = true;
        state.time =
            std::max(0.0, applied.time + static_cast<f64>(frames) / 60.0);
        Sync();
    }

    void EditorSession::showTime() {
        if(content.ApplyTime(state.time))
            inspectorDirty = true;
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

    void EditorSession::SelectObject(std::optional<usize> object) {
        const auto objects = content.Objects();
        if(object && *object >= objects.size())
            return;

        state.selected = object ? objects[*object].name : Str{};
        Sync();
    }

    void EditorSession::select(std::optional<usize> object) {
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
                section.apply = [] {
                };
        }
        exposeSelection();
        bindGizmo();
        refreshGizmo();
        selectionChanged = true;
        Report(
            selection ? std::format("selected {}", state.selected)
                      : Str("nothing selected")
        );
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

        return layoutGizmo(
            camera,
            viewport,
            *gizmoRow->position,
            yaw,
            gizmoRow->parts
        );
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

        port.expose(StateTarget, &state, *getDesc<EditorState>(), [this] {
            Sync();
        });
        port.expose(CameraTarget, &camera, *getDesc<EditorCamera>(), [this] {
            camera.RecomputeView();
            leaveCut();
            refreshGizmo();
        });
        // read-only: a write is put back
        port.expose(GizmoTarget, &gizmoView, *getDesc<GizmoView>(), [this] {
            refreshGizmo();
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
            Report(
                std::format(
                    "no cut '{}'; there are {}, or {}",
                    state.cut,
                    join(cuts, ", ", &EditorCut::name),
                    FreeCut
                )
            );
            return false;
        }

        camera.Snap(*found);
        camera.TakeMoved();
        Report(std::format("cut {}", state.cut));

        return true;
    }

    bool EditorSession::applyKey() {
        const auto keys = content.Keys();
        if(!std::ranges::contains(keys, state.key)) {
            Report(
                std::format(
                    "no key '{}'; there are {}",
                    state.key,
                    join(keys, ", ")
                )
            );
            return false;
        }

        clearColor(content.ApplyKey(state.key));
        // a key rewrites rows the inspector may show
        inspectorDirty = true;
        Report(std::format("key {}", state.key));

        return true;
    }

    bool EditorSession::applySelected() {
        if(state.selected.empty()) {
            select(std::nullopt);
            return true;
        }

        const auto object = objectNamed(content.Objects(), state.selected);
        if(!object) {
            Report(object.error());
            return false;
        }

        select(*object);
        return true;
    }

    void EditorSession::applyPickAt() {
        const auto pixel = std::exchange(state.pickAt, EditorNoPick);
        if(const auto light = markerAt(pixel)) {
            select(light);
            return;
        }

        std::optional<PickHit> hit;
        if(const auto ray = rayThroughPixel(camera, pixel, viewport)) {
            hit = pickScene(scene, *ray, [this](PrimitiveHandle primitive) {
                return content.MeshesOf(primitive);
            });
        }
        select(hit ? content.ObjectOf(hit->primitive) : std::nullopt);
    }

    Str EditorSession::HeldChange() const {
        if(!hold || !gizmoRow)
            return {};

        const auto& drag = hold->drag;
        if(isGizmoArrow(drag.handle))
            return std::format(
                "{:+.2f} m",
                dot(*gizmoRow->position - hold->position, drag.axis)
            );
        if(drag.handle == GizmoHandle::Ring) {
            const auto turn =
                wrapDegrees(*gizmoRow->yaw - hold->yaw + 180.0f) - 180.0f;
            return std::format("{:+.1f} deg", turn);
        }

        const auto axis = gizmoAxisIndex(drag.handle).value_or(0);
        const auto factor = (*gizmoRow->scale)[axis] / hold->scale[axis];

        return std::format("x{:.2f}", factor);
    }

    void EditorSession::reloadScene() {
        state.reload = false;
        if(const auto read = content.ReadScene(state.scene); !read) {
            Report(
                std::format(
                    "reload refused, revision {} stays: {}",
                    applied.revision,
                    read.error()
                )
            );
            state.scene = loadedScene;
            applied = state;
            return;
        }

        Str notes;
        const auto name = state.selected;
        std::optional<EditorCut> standing;
        if(state.cut != FreeCut) {
            const auto cuts = content.Cuts();
            if(const auto found =
                   std::ranges::find(cuts, state.cut, &EditorCut::name);
               found != cuts.end())
                standing = *found;
        }
        if(hold) {
            notes += std::format("; let go of {}", enumName(Held()));
            release();
        }
        // nothing may point into the old rows once the content swaps them
        unexposeSelection();
        inspected.clear();
        gizmoRow.reset();
        selection.reset();
        const auto summary = content.SwapScene();

        const auto keys = content.Keys();
        if(!std::ranges::contains(keys, state.key)) {
            notes += std::format(
                "; key {} is gone, showing {}",
                state.key,
                keys.front()
            );
            state.key = keys.front();
        }
        clearColor(content.ApplyKey(state.key));
        // the rows come back at frame 0; the scene shows its current time
        showTime();

        // the cut keeps its name only where it still stands; the camera stays
        if(standing) {
            const auto cuts = content.Cuts();
            const auto found =
                std::ranges::find(cuts, standing->name, &EditorCut::name);
            if(found == cuts.end() || *found != *standing) {
                notes += std::format(
                    "; cut {} changed, the camera stays",
                    standing->name
                );
                leaveCut();
            }
        }

        if(!name.empty()) {
            const auto object = objectNamed(content.Objects(), name);
            select(object ? std::optional(*object) : std::nullopt);
            if(!object)
                notes += std::format("; {} is gone", name);
        }

        state.revision = applied.revision + 1;
        loadedScene = state.scene;
        inspectorDirty = true;
        selectionChanged = true;
        Report(
            std::format(
                "reloaded {}: revision {}, {}{}",
                state.scene,
                state.revision,
                summary,
                notes
            )
        );
        refreshGizmo();
        applied = state;
    }

    void EditorSession::applyGrab() {
        const auto pixel = std::exchange(state.grab, EditorNoPick);
        release();
        const auto layout = SelectionGizmo();
        if(!layout) {
            Report(
                chromeShown ? "nothing selected carries a gizmo in view"
                            : "the gizmo shows with debug.showPanel"
            );
            return;
        }

        const auto handle = hoverGizmo(projectGizmo(*layout), pixel);
        const auto ray = rayThroughPixel(camera, pixel, viewport);
        const auto drag = handle != GizmoHandle::None && ray
                              ? grabGizmo(*layout, handle, *ray)
                              : std::nullopt;
        if(!drag) {
            Report(std::format("no handle at {}, {}", pixel.x, pixel.y));
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
        Report(std::format("holding {}", enumName(handle)));
    }

    void EditorSession::applyDrag() {
        const auto pixel = std::exchange(state.drag, EditorNoPick);
        if(!hold) {
            Report("no handle is held; write editor.grab first");
            return;
        }

        const auto ray = rayThroughPixel(camera, pixel, viewport);
        const auto edit =
            ray ? dragGizmo(hold->drag, *ray, state.snap) : std::nullopt;
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
        Report(
            "editor.handle names the held handle: write editor.grab to take "
            "one, None to let go"
        );
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
        // the accessor a port write goes through, checked for the type it must
        // be
        const auto field = [&](CStr name, const TypeOps* type) {
            const auto resolved =
                resolveProperty(section.target, *section.desc, name);
            return resolved.type == type ? resolved.member : nullptr;
        };
        GizmoRow row{
            .position =
                static_cast<Vec3*>(field("position", getTypeOps<Vec3>())),
            .yaw = static_cast<f32*>(field("yaw", getTypeOps<f32>())),
            .scale = static_cast<Vec3*>(field("scale", getTypeOps<Vec3>())),
            .parts = section.gizmo
        };
        CROWY_ASSERT(
            row.position != nullptr,
            "a section offering a gizmo has a Vec3 position"
        );
        CROWY_ASSERT(
            row.yaw != nullptr || !hasFlag(row.parts, GizmoParts::Turn),
            "the ring needs an f32 yaw"
        );
        CROWY_ASSERT(
            row.scale != nullptr ||
                !(hasFlag(row.parts, GizmoParts::Scale) ||
                  hasFlag(row.parts, GizmoParts::ScaleAxes)),
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

        gizmoView.pivot =
            projectToWindow(layout->viewProj, layout->pivot, layout->viewport)
                .value_or(EditorNoPick);
        gizmoView.length = layout->length;
        for(const auto& [handle, name, aim]: GizmoViewAims)
            gizmoView.*aim = aimGizmo(*layout, handle).value_or(GizmoAim{});
    }

    void EditorSession::wroteSection(usize section) {
        inspected[section].apply();
        inspectorDirty = true;
        refreshGizmo();
    }

    std::vector<MarkedLight> EditorSession::MarkedLights() const {
        std::vector<MarkedLight> marked;
        for(usize i = 0; i < content.Objects().size(); ++i) {
            if(const auto light = content.LightOf(i);
               light && scene.Lights().IsValid(*light))
                marked.push_back(MarkedLight{.object = i, .light = *light});
        }

        return marked;
    }

    std::optional<usize> EditorSession::markerAt(Vec2 pixel) const {
        const auto marked = MarkedLights();
        std::vector<Vec3> markers;
        for(const auto& [object, light]: marked)
            markers.push_back(scene.Lights().GetRef(light).position);
        const auto viewProj = camera.ViewProj(viewport.x / viewport.y);
        const auto nearest =
            nearestOnScreen(viewProj, viewport, pixel, markers, MarkerRadius);

        return nearest ? std::optional(marked[*nearest].object) : std::nullopt;
    }

    void EditorSession::exposeSelection() {
        if(!port.expose)
            return;

        for(usize i = 0; i < inspected.size(); ++i) {
            const auto& section = inspected[i];
            auto name =
                i == 0 ? Str(SelectionTarget)
                       : std::format("{}.{}", SelectionTarget, section.label);
            port.expose(name, section.target, *section.desc, [this, i] {
                wroteSection(i);
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

    // clang-format off
    CROWY_STRUCT(EditorState)
        .AddProperty("cut", &EditorState::cut)
        .AddProperty("key", &EditorState::key)
        .AddProperty("selected", &EditorState::selected)
        .AddProperty("pickAt", &EditorState::pickAt)
        .AddProperty("grab", &EditorState::grab)
        .AddProperty("drag", &EditorState::drag)
        .AddProperty("handle", &EditorState::handle)
        .AddProperty("snap", &EditorState::snap)
        .AddProperty("cancel", &EditorState::cancel)
        .AddProperty("paused", &EditorState::paused)
        .AddProperty("time", &EditorState::time)
        .AddProperty("scene", &EditorState::scene)
        .AddProperty("reload", &EditorState::reload)
        .AddProperty("revision", &EditorState::revision)
        .AddProperty("status", &EditorState::status)
    CROWY_STRUCT_END(EditorState)

    // clang-format on

    namespace
    {
        // not CROWY_STRUCT: the aims register in a loop over GizmoViewAims
        const auto IsGizmoViewRegistered = [] {
            auto view = reflectStruct<GizmoView>();
            view.SetName("GizmoView")
                .AddProperty("pivot", &GizmoView::pivot)
                .AddProperty("length", &GizmoView::length);
            for(const auto& [handle, name, aim]: GizmoViewAims)
                view.AddProperty(name, aim);

            return view.Build();
        }();
    }
}
