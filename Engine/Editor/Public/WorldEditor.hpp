#pragma once

#include <optional>
#include <vector>

#include "CommandPort.hpp"
#include "EditorCamera.hpp"
#include "EditorGizmo.hpp"
#include "EditorPick.hpp"
#include "PrimitiveActors.hpp"
#include "World.hpp"

namespace Crowy
{
    // what the inspector shows of the selected actor, found again at every
    // access
    struct ActorSection {
        Str label;
        PropertyTarget target;
    };

    // viewport points, EditorNoPick where a handle is not shown
    struct GizmoAims {
        Vec2 pivot = EditorNoPick;
        // meters an unforeshortened arrow spans
        f32 length = 0.0f;
        GizmoAim moveX;
        GizmoAim moveY;
        GizmoAim moveZ;
        GizmoAim ring;
        GizmoAim ringX;
        GizmoAim ringZ;
        GizmoAim scaleX;
        GizmoAim scaleY;
        GizmoAim scaleZ;
    };

    // The input and the port write a field and call Sync, which applies what
    // changed and reverts what it cannot apply.
    struct WorldEditorState {
        // an actor's display name, empty for none
        Str selected;
        // pulses in viewport points: every write acts, then reads back as
        // EditorNoPick
        Vec2 pickAt = EditorNoPick;
        Vec2 grab = EditorNoPick;
        Vec2 drag = EditorNoPick;
        GizmoHandle handle = GizmoHandle::None;
        bool snap = false;
        bool cancel = false;
        Str status;
    };

    // Selects a World's actors in the view an EditorCamera sees, and edits
    // them in memory through their transform and component targets.
    class WorldEditor {
    public:
        static constexpr CStr StateTarget = "editor";
        static constexpr CStr GizmoTarget = "gizmo";
        // the transform's; a component's is "selection.<label>"
        static constexpr CStr SelectionTarget = "selection";
        // viewport points
        static constexpr f32 MarkerRadius = 8.0f;

    private:
        struct GizmoHold {
            GizmoDrag drag;
            Transform start;
        };

        World& world;
        const RenderScene& scene;
        const EditorCamera& camera;
        const PrimitiveActors& primitives;
        CommandPort* port;
        WorldEditorState state;
        // what was last applied, which a write is compared against
        WorldEditorState applied;
        std::optional<ActorId> selection;
        // the view's size, while one shows
        std::optional<Vec2> viewport;
        std::vector<ActorSection> sections;
        std::optional<GizmoHold> hold;
        GizmoAims aims;
        bool selectionChanged = false;
        // raised when anything but the panel changed what the inspector shows
        bool inspectorDirty = false;
        // what the pick last read
        mutable std::vector<const MeshData*> meshes;
        // after everything their callbacks reach
        PortExposures exposures;
        PortExposures selectionExposures;

    public:
        ~WorldEditor() = default;
        CROWY_DECLARE_PINNED(WorldEditor)

        // the World provides the PrimitiveActors; a null port exposes nothing
        WorldEditor(
            World& world,
            const RenderScene& scene,
            const EditorCamera& camera,
            CommandPort* port = nullptr
        );

        void Sync();
        // nullopt while no view shows, which lets go of a held handle
        void Update(std::optional<Vec2> viewSize);
        void Select(std::optional<ActorId> actor);
        void PickAt(Vec2 point);
        // whether a handle took the press
        bool Grab(Vec2 point);
        void DragTo(Vec2 point, bool snap);
        void Release();
        void Cancel();
        void Report(Str status);

        Str DisplayName(ActorId actor) const;
        std::optional<GizmoLayout> SelectionGizmo() const;
        // the actors with no mesh to click, marked where they stand
        std::vector<ActorId> MarkedActors() const;
        bool TakeSelectionChanged() noexcept;
        bool TakeInspectorDirty() noexcept;

        std::optional<ActorId> Selection() const noexcept { return selection; }
        const std::vector<ActorSection>& Sections() const noexcept {
            return sections;
        }
        GizmoHandle Held() const noexcept {
            return hold ? hold->drag.handle : GizmoHandle::None;
        }
        const GizmoAims& Gizmo() const noexcept { return aims; }
        WorldEditorState& State() noexcept { return state; }

    private:
        bool inView(Vec2 point) const;
        MeshList meshesOf(PrimitiveHandle primitive) const;
        std::optional<ActorId> markerAt(Vec2 point) const;
        void select(std::optional<ActorId> actor);
        bool applySelected();
        void applyPickAt();
        void applyGrab();
        void applyDrag();
        void applyHandle();
        void applyCancel();
        void release();
        void refreshGizmo();
    };
}
