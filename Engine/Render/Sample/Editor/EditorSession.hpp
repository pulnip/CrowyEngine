#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ClassRegistry.hpp"
#include "EditorCamera.hpp"
#include "EditorGizmo.hpp"
#include "EditorPick.hpp"
#include "Primitives.hpp"
#include "PropertyWrite.hpp"
#include "RenderScene.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class EditorContent;
    class EditorSession;
    struct EditorObject;
    struct EditorPort;
    struct InspectSection;

    using ClearColorSink = std::function<void(Color)>;
    using InspectSections = std::vector<InspectSection>;
    using EditorObjects = std::span<const EditorObject>;
    using PortExpose =
        std::function<void(StrView, void*, const TypeDesc&, DirtyCallback)>;
    using PortUnexpose = std::function<void(StrView)>;

    enum class EditorObjectKind : u8 {
        Instance,
        Quad,
        Light,
    };

    struct EditorObject {
        // "<kind>/<name>", unique across the content
        Str name;
        Str group;
        Str detail;
        EditorObjectKind kind = EditorObjectKind::Instance;
    };

    // an empty apply means the write lands in the scene's own row
    struct InspectSection {
        Str label;
        void* target = nullptr;
        const TypeDesc* desc = nullptr;
        DirtyCallback apply;
        // only the first section's counts
        GizmoParts gizmo = GizmoParts::None;
    };

    // EditorNoPick where a handle is not shown
    struct GizmoView {
        Vec2 pivot = EditorNoPick;
        // meters an unforeshortened arrow spans
        f32 length = 0.0f;
        GizmoAim moveX;
        GizmoAim moveY;
        GizmoAim moveZ;
        GizmoAim ring;
        GizmoAim scale;
        GizmoAim scaleX;
        GizmoAim scaleY;
        GizmoAim scaleZ;
    };

    struct EditorPort {
        PortExpose expose;
        PortUnexpose unexpose;
    };

    // The panel, the keyboard and the port all write a field and call Sync,
    // which applies what changed and reverts what it cannot apply.
    struct EditorState {
        Str cut;
        Str key;
        Str selected;
        // pulses in window points: every write acts, then reads back as
        // EditorNoPick
        Vec2 pickAt = EditorNoPick;
        Vec2 grab = EditorNoPick;
        Vec2 drag = EditorNoPick;
        GizmoHandle handle = GizmoHandle::None;
        bool snap = false;
        bool cancel = false;
        bool paused = true;
        f64 time = 0.0;
        Str scene;
        bool reload = false;
        u32 revision = 1;
        Str status;
    };

    class EditorContent {
    public:
        CROWY_DECLARE_INTERFACE(EditorContent)

        virtual std::span<const EditorCut> Cuts() const = 0;
        virtual std::span<const Str> Keys() const = 0;
        // returns the key's clear color
        virtual Color ApplyKey(StrView key) = 0;

        // stable until the content reloads
        virtual EditorObjects Objects() const = 0;
        virtual std::optional<usize> ObjectOf(
            PrimitiveHandle primitive
        ) const = 0;
        virtual std::optional<PrimitiveHandle> PrimitiveOf(
            usize object
        ) const = 0;
        virtual std::optional<LightHandle> LightOf(usize object) const = 0;
        virtual MeshList MeshesOf(PrimitiveHandle primitive) const = 0;
        // the targets stay put until a reload
        virtual InspectSections Inspect(usize object) = 0;
        // changes nothing; SwapScene puts an accepted file in place
        virtual Str ReadScene(StrView file) = 0;
        // every target and apply Inspect gave dies; returns a summary line
        virtual Str SwapScene() = 0;
        // whether a row changed
        virtual bool ApplyTime(f64 seconds) = 0;
    };

    class EditorSession {
    public:
        static constexpr CStr FreeCut = "free";
        static constexpr CStr StateTarget = "editor";
        static constexpr CStr CameraTarget = "camera";
        static constexpr CStr GizmoTarget = "gizmo";
        // the first section's; the others are "selection.<label>"
        static constexpr CStr SelectionTarget = "selection";
        // window points
        static constexpr f32 MarkerRadius = 8.0f;

    private:
        struct GizmoRow {
            Vec3* position = nullptr;
            f32* yaw = nullptr;
            Vec3* scale = nullptr;
            GizmoParts parts = GizmoParts::None;
        };

        struct GizmoHold {
            GizmoDrag drag;
            Vec3 position{};
            f32 yaw = 0.0f;
            Vec3 scale{1.0f, 1.0f, 1.0f};
        };

        EditorCamera& camera;
        EditorContent& content;
        const RenderScene& scene;
        const bool& chromeShown;
        Str loadedScene;
        ClearColorSink clearColor;
        EditorPort port;
        EditorState state;
        // what was last applied, which a write is compared against
        EditorState applied;
        std::optional<usize> selection;
        Vec2 viewport{1.0f, 1.0f};
        InspectSections inspected;
        std::vector<Str> exposed;
        bool selectionChanged = false;
        // raised when anything but the panel changed what the inspector shows
        bool inspectorDirty = false;
        std::optional<GizmoRow> gizmoRow;
        std::optional<GizmoHold> hold;
        GizmoView gizmoView;

    public:
        ~EditorSession();
        CROWY_DECLARE_PINNED(EditorSession)

        EditorSession(
            EditorCamera& camera,
            EditorContent& content,
            const RenderScene& scene,
            const bool& chromeShown,
            ClearColorSink clearColor,
            EditorPort port = {}
        );

        // throws std::runtime_error when the cut or key is not the content's
        void Start(StrView cut, StrView key, StrView scene);
        void Sync();
        // a frame's input already reached the camera
        void Update(Vec2 windowSize);
        void SetViewport(Vec2 windowSize);
        // outside any port callback
        void Advance(f64 seconds);
        void Reload();
        void TogglePause();
        void Report(Str status);
        // sixtieths of a second, whatever the display's rate
        void StepTime(i32 frames);

        void SelectCut(usize index);
        void SelectKey(usize index);
        void Select(std::optional<usize> object);
        void PickAt(Vec2 pixel);
        // whether a handle is held after it
        bool Grab(Vec2 pixel);
        void DragTo(Vec2 pixel, bool snap);
        void Release();
        void Cancel();

        EditorState& State() noexcept { return state; }
        const EditorState& State() const noexcept { return state; }
        std::optional<usize> Selection() const noexcept { return selection; }
        const EditorContent& Content() const noexcept { return content; }
        const EditorCamera& Camera() const noexcept { return camera; }
        Vec2 Viewport() const noexcept { return viewport; }
        bool TakeSelectionChanged() noexcept;
        bool TakeInspectorDirty() noexcept;
        const InspectSections& Inspected() const noexcept { return inspected; }
        GizmoHandle Held() const noexcept {
            return hold ? hold->drag.handle : GizmoHandle::None;
        }
        const GizmoView& Gizmo() const noexcept { return gizmoView; }
        // panels built in another revision point at rows a reload freed
        u32 Revision() const noexcept { return applied.revision; }
        std::optional<GizmoLayout> SelectionGizmo() const;
        Str HeldChange() const;

    private:
        void exposeTargets();
        void leaveCut();
        bool applyCut();
        bool applyKey();
        bool applySelected();
        void applyPickAt();
        // the two-phase reload: read and check, then let go of every pointer
        // into the old rows before the content swaps them
        void reloadScene();
        void showTime();
        void applyGrab();
        void applyDrag();
        void applyHandle();
        void applyCancel();
        void release();
        void bindGizmo();
        void refreshGizmo();
        void wroteSection(usize section);
        std::optional<usize> markerAt(Vec2 pixel) const;
        void report(Str status);
        void reportSelection();
        void exposeSelection();
        void unexposeSelection();
    };
}
