#include "WorldEditor.hpp"

#include <array>
#include <format>
#include <utility>

#include "ClassRegistry.hpp"
#include "Geometry/Projection.hpp"
#include "MeshAsset.hpp"

namespace Crowy
{
    namespace
    {
        constexpr auto ActorParts =
            combine(GizmoParts::Move, GizmoParts::Turns, GizmoParts::ScaleAxes);

        struct NamedAim {
            GizmoHandle handle = GizmoHandle::None;
            CStr name = nullptr;
            GizmoAim GizmoAims::* aim = nullptr;
        };

        constexpr std::array<NamedAim, 9> NamedAims{{
            {GizmoHandle::MoveX, "moveX", &GizmoAims::moveX},
            {GizmoHandle::MoveY, "moveY", &GizmoAims::moveY},
            {GizmoHandle::MoveZ, "moveZ", &GizmoAims::moveZ},
            {GizmoHandle::Ring, "ring", &GizmoAims::ring},
            {GizmoHandle::RingX, "ringX", &GizmoAims::ringX},
            {GizmoHandle::RingZ, "ringZ", &GizmoAims::ringZ},
            {GizmoHandle::ScaleX, "scaleX", &GizmoAims::scaleX},
            {GizmoHandle::ScaleY, "scaleY", &GizmoAims::scaleY},
            {GizmoHandle::ScaleZ, "scaleZ", &GizmoAims::scaleZ},
        }};

        // the parent's world transform, which the actor's local one sits in
        struct ParentFrame {
            Mat4 toWorld = unitMat();
            Vec4 rotation = unitQuat();
        };

        ParentFrame frameOf(const World& world, ActorId actor) {
            const auto parent = world.Parent(actor);
            if(!parent.IsValid())
                return {};

            return ParentFrame{
                .toWorld = world.WorldMatrix(parent),
                .rotation = world.WorldRotation(parent)
            };
        }
    }

    WorldEditor::WorldEditor(
        World& world,
        const RenderScene& scene,
        const EditorCamera& camera,
        CommandPort* port
    )
        : world(world),
          scene(scene),
          camera(camera),
          primitives(world.Service<PrimitiveActors>()),
          port(port),
          exposures(port),
          selectionExposures(port) {
        exposures.Expose(
            StateTarget,
            &state,
            *getDesc<WorldEditorState>(),
            [this] { Sync(); }
        );
        exposures.Expose(GizmoTarget, &aims, *getDesc<GizmoAims>(), [this] {
            refreshGizmo();
        });
    }

    void WorldEditor::Sync() {
        if(state.status != applied.status)
            state.status = applied.status;
        if(state.pickAt != EditorNoPick)
            applyPickAt();
        else if(state.selected != applied.selected && !applySelected())
            state.selected = applied.selected;
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

    void WorldEditor::Update(std::optional<Vec2> viewSize) {
        viewport = viewSize;
        if(selection && !world.IsValid(*selection))
            select(std::nullopt);
        if(hold && !viewport)
            release();
        refreshGizmo();
    }

    void WorldEditor::Select(std::optional<ActorId> actor) {
        select(actor);
    }

    void WorldEditor::PickAt(Vec2 point) {
        state.pickAt = point;
        Sync();
    }

    bool WorldEditor::Grab(Vec2 point) {
        state.grab = point;
        Sync();

        return hold.has_value();
    }

    void WorldEditor::DragTo(Vec2 point, bool snap) {
        state.snap = snap;
        state.drag = point;
        Sync();
    }

    void WorldEditor::Release() {
        state.handle = GizmoHandle::None;
        Sync();
    }

    void WorldEditor::Cancel() {
        state.cancel = true;
        Sync();
    }

    void WorldEditor::Report(Str status) {
        state.status = std::move(status);
        applied.status = state.status;
    }

    Str WorldEditor::DisplayName(ActorId actor) const {
        const auto& name = world.Name(actor);

        return name.empty() ? std::format("actor-{}", world.FileId(actor))
                            : name;
    }

    std::optional<GizmoLayout> WorldEditor::SelectionGizmo() const {
        if(!viewport || !selection)
            return std::nullopt;

        const auto frame = frameOf(world, *selection);
        const auto& local = world.LocalTransform(*selection);

        return layoutGizmo(
            camera,
            *viewport,
            transformPoint(frame.toWorld, local.position),
            quat(frame.rotation, local.rotation),
            ActorParts
        );
    }

    std::vector<ActorId> WorldEditor::MarkedActors() const {
        std::vector<ActorId> marked;
        world.EachActor([&](ActorId actor, ActorId) {
            if(!primitiveOfActor(world, actor))
                marked.push_back(actor);
        });

        return marked;
    }

    bool WorldEditor::TakeSelectionChanged() noexcept {
        return std::exchange(selectionChanged, false);
    }

    bool WorldEditor::TakeInspectorDirty() noexcept {
        return std::exchange(inspectorDirty, false);
    }

    bool WorldEditor::inView(Vec2 point) const {
        return viewport && point.x >= 0.0f && point.y >= 0.0f &&
               point.x < viewport->x && point.y < viewport->y;
    }

    MeshList WorldEditor::meshesOf(PrimitiveHandle primitive) const {
        meshes.clear();
        const auto& resource =
            scene.Meshes().GetRef(scene.Primitives().GetRef(primitive).mesh);
        if(!primitives.ActorOf(primitive) || !resource.source)
            return {};

        for(const auto& slot: resource.source->slots)
            meshes.push_back(&slot.mesh);

        return meshes;
    }

    std::optional<ActorId> WorldEditor::markerAt(Vec2 point) const {
        const auto marked = MarkedActors();
        std::vector<Vec3> positions;
        for(const auto actor: marked)
            positions.push_back(world.WorldPosition(actor));
        const auto nearest = nearestOnScreen(
            camera.ViewProj(viewport->x / viewport->y),
            *viewport,
            point,
            positions,
            MarkerRadius
        );

        return nearest ? std::optional(marked[*nearest]) : std::nullopt;
    }

    void WorldEditor::select(std::optional<ActorId> actor) {
        release();
        selectionExposures = PortExposures(port);
        sections.clear();
        selection = actor;
        state.selected = actor ? DisplayName(*actor) : Str{};
        applied.selected = state.selected;
        if(actor) {
            sections.push_back({"transform", world.TransformTarget(*actor)});
            world.EachComponent(
                *actor,
                [&](const ComponentInfo& info, const void*) {
                    if(!info.desc->PropertiesInChain().empty())
                        sections.push_back(
                            {info.desc->name, world.Target(*actor, *info.desc)}
                        );
                }
            );
        }
        for(usize i = 0; i < sections.size(); ++i) {
            selectionExposures.Expose(
                i == 0
                    ? Str(SelectionTarget)
                    : std::format("{}.{}", SelectionTarget, sections[i].label),
                sections[i].target,
                [this] {
                    inspectorDirty = true;
                    refreshGizmo();
                }
            );
        }
        refreshGizmo();
        selectionChanged = true;
        Report(
            actor ? std::format("selected {}", state.selected)
                  : Str("nothing selected")
        );
    }

    bool WorldEditor::applySelected() {
        if(state.selected.empty()) {
            select(std::nullopt);
            return true;
        }

        std::optional<ActorId> found;
        bool repeated = false;
        world.EachActor([&](ActorId actor, ActorId) {
            if(DisplayName(actor) != state.selected)
                return;

            repeated = repeated || found.has_value();
            found = actor;
        });
        if(repeated || !found) {
            Report(
                repeated ? std::format(
                               "'{}' names more than one actor",
                               state.selected
                           )
                         : std::format("no actor '{}'", state.selected)
            );
            return false;
        }

        select(found);
        return true;
    }

    void WorldEditor::applyPickAt() {
        const auto point = std::exchange(state.pickAt, EditorNoPick);
        if(!inView(point)) {
            Report(std::format("{}, {} is outside the view", point.x, point.y));
            return;
        }
        if(const auto marked = markerAt(point)) {
            select(marked);
            return;
        }

        std::optional<ActorId> actor;
        if(const auto ray = rayThroughPixel(camera, point, *viewport)) {
            const auto hit =
                pickScene(scene, *ray, [this](PrimitiveHandle primitive) {
                    return meshesOf(primitive);
                });
            if(hit)
                actor = primitives.ActorOf(hit->primitive);
        }
        select(actor);
    }

    void WorldEditor::applyGrab() {
        const auto point = std::exchange(state.grab, EditorNoPick);
        release();
        const auto layout = SelectionGizmo();
        if(!layout) {
            Report("nothing selected carries a gizmo in view");
            return;
        }

        const auto handle = inView(point)
                                ? hoverGizmo(projectGizmo(*layout), point)
                                : GizmoHandle::None;
        const auto ray = rayThroughPixel(camera, point, *viewport);
        const auto drag = handle != GizmoHandle::None && ray
                              ? grabGizmo(*layout, handle, *ray)
                              : std::nullopt;
        if(!drag) {
            Report(std::format("no handle at {}, {}", point.x, point.y));
            return;
        }

        hold =
            GizmoHold{.drag = *drag, .start = world.LocalTransform(*selection)};
        state.handle = handle;
        Report(std::format("holding {}", enumName(handle)));
    }

    void WorldEditor::applyDrag() {
        const auto point = std::exchange(state.drag, EditorNoPick);
        if(!hold) {
            Report("no handle is held; write editor.grab first");
            return;
        }

        const auto ray = rayThroughPixel(camera, point, *viewport);
        const auto edit =
            ray ? dragGizmo(hold->drag, *ray, state.snap) : std::nullopt;
        // a ray that misses the plane leaves the last change standing
        if(!edit)
            return;

        const auto& drag = hold->drag;
        const auto& start = hold->start;
        const auto frame = frameOf(world, *selection);
        auto local = world.LocalTransform(*selection);
        if(isGizmoArrow(drag.handle)) {
            local.position =
                start.position +
                transformDirection(inverseAffine(frame.toWorld), edit->offset);
        }
        else if(isGizmoRing(drag.handle)) {
            const auto axis = rotate(drag.axis, conjugate(frame.rotation));
            local.rotation = normalize(
                quat(axisAngle(axis, radiansOf(edit->turn)), start.rotation)
            );
        }
        else {
            local.scale = start.scale * edit->factor;
        }
        world.SetLocalTransform(*selection, local);
        inspectorDirty = true;
    }

    void WorldEditor::applyHandle() {
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

    void WorldEditor::applyCancel() {
        state.cancel = false;
        if(!hold)
            return;

        const auto start = hold->start;
        release();
        world.SetLocalTransform(*selection, start);
        inspectorDirty = true;
    }

    void WorldEditor::release() {
        hold.reset();
        state.handle = GizmoHandle::None;
    }

    void WorldEditor::refreshGizmo() {
        aims = GizmoAims{};
        const auto layout = SelectionGizmo();
        if(!layout)
            return;

        aims.pivot =
            projectToWindow(layout->viewProj, layout->pivot, layout->viewport)
                .value_or(EditorNoPick);
        aims.length = layout->length;
        for(const auto& [handle, name, aim]: NamedAims)
            aims.*aim = aimGizmo(*layout, handle).value_or(GizmoAim{});
    }

    // clang-format off
    CROWY_STRUCT(WorldEditorState)
        .AddProperty("selected", &WorldEditorState::selected)
        .AddProperty("pickAt", &WorldEditorState::pickAt)
        .AddProperty("grab", &WorldEditorState::grab)
        .AddProperty("drag", &WorldEditorState::drag)
        .AddProperty("handle", &WorldEditorState::handle)
        .AddProperty("snap", &WorldEditorState::snap)
        .AddProperty("cancel", &WorldEditorState::cancel)
        .AddProperty("status", &WorldEditorState::status)
    CROWY_STRUCT_END(WorldEditorState)
    // clang-format on

    namespace
    {
        // not CROWY_STRUCT: the aims register in a loop over NamedAims
        const auto IsGizmoAimsRegistered = [] {
            auto view = reflectStruct<GizmoAims>();
            view.SetName("GizmoAims")
                .AddProperty("pivot", &GizmoAims::pivot)
                .AddProperty("length", &GizmoAims::length);
            for(const auto& [handle, name, aim]: NamedAims)
                view.AddProperty(name, aim);

            return view.Build();
        }();
    }
}
