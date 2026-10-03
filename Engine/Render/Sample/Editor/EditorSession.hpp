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
    using PortExpose = std::function<void(StrView, void*, const TypeDesc&, DirtyCallback)>;
    using PortUnexpose = std::function<void(StrView)>;

    // what the editor can select: one row of the content, named across kinds
    enum class EditorObjectKind : u8 {
        Instance,
        Quad,
        Light,
    };

    struct EditorObject {
        // "<kind>/<name>", unique across the content
        Str name;
        // the hierarchy's folder
        Str group;
        // what the row draws or belongs to, for the hierarchy and its search
        Str detail;
        EditorObjectKind kind = EditorObjectKind::Instance;
    };

    // one editable part of an object: a reflected row and what turns a write
    // into it into scene rows; an empty apply means the row is the scene's
    struct InspectSection {
        Str label;
        void* target = nullptr;
        const TypeDesc* desc = nullptr;
        DirtyCallback apply;
        // what the gizmo offers on the row; only the first section's counts
        GizmoParts gizmo = GizmoParts::None;
    };

    // the selection's handles on screen, for a script to press where a person
    // sees them; EditorNoPick where one is not shown, and a write is restored
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

    // the port as the session uses it, empty where there is none
    struct EditorPort {
        PortExpose expose;
        PortUnexpose unexpose;
    };

    // The panel, the keyboard and the port all write a field and call Sync,
    // which applies what changed and reverts what it cannot apply.
    struct EditorState {
        // a cut's name, or "free" once the camera flies
        Str cut;
        // a lighting key's name
        Str key;
        // an object's name, or empty; a unique bare name is accepted
        Str selected;
        // a pixel to pick at, window points from the top-left; every write
        // picks, and the field reads back as EditorNoPick
        Vec2 pickAt = EditorNoPick;
        // a pixel to press the gizmo at; every write takes the handle under
        // it, or none, and reads back as EditorNoPick
        Vec2 grab = EditorNoPick;
        // a pixel to drag the held handle to, measured from the grab; every
        // write moves the selection and reads back as EditorNoPick
        Vec2 drag = EditorNoPick;
        // the held handle; None releases where the drag stands, and any other
        // write is reverted
        GizmoHandle handle = GizmoHandle::None;
        // drags snap their change: 0.25 m, 15 degrees, x0.1
        bool snap = false;
        // a write of true puts the row back as the grab found it and releases
        bool cancel = false;
        // the last refusal or outcome; a write is reverted
        Str status;
    };

    // What the editor asks of the content it edits; nothing in the editor
    // knows which content that is.
    class EditorContent {
    public:
        CROWY_DECLARE_INTERFACE(EditorContent)

        virtual std::span<const EditorCut> Cuts() const = 0;
        virtual std::span<const Str> Keys() const = 0;
        // writes the key into the scene's rows; returns its clear color
        virtual Color ApplyKey(StrView key) = 0;

        // stable until the content reloads
        virtual EditorObjects Objects() const = 0;
        virtual std::optional<usize> ObjectOf(PrimitiveHandle primitive) const = 0;
        virtual std::optional<PrimitiveHandle> PrimitiveOf(usize object) const = 0;
        virtual std::optional<LightHandle> LightOf(usize object) const = 0;
        // the primitive's meshes in its model space, for picking
        virtual MeshList MeshesOf(PrimitiveHandle primitive) const = 0;
        // what the inspector edits; the targets stay put until a reload
        virtual InspectSections Inspect(usize object) = 0;
    };

    class EditorSession {
    public:
        // the state the camera reports once it has flown off a cut
        static constexpr CStr FreeCut = "free";
        // the port's names for the state, the camera and the gizmo's aims
        static constexpr CStr StateTarget = "editor";
        static constexpr CStr CameraTarget = "camera";
        static constexpr CStr GizmoTarget = "gizmo";
        // the port's name for the selection's first section; the others are
        // "selection.<label>"
        static constexpr CStr SelectionTarget = "selection";
        // a click this near a light's marker picks the light, ahead of geometry
        static constexpr f32 MarkerRadius = 8.0f;

    private:
        // the selection's row as the gizmo writes it, resolved as a port
        // write resolves it
        struct GizmoRow {
            Vec3* position = nullptr;
            f32* yaw = nullptr;
            Vec3* scale = nullptr;
            GizmoParts parts = GizmoParts::None;
        };

        // a held handle and the row as the grab found it
        struct GizmoHold {
            GizmoDrag drag;
            Vec3 position{};
            f32 yaw = 0.0f;
            Vec3 scale{1.0f, 1.0f, 1.0f};
        };

        EditorCamera& camera;
        EditorContent& content;
        const RenderScene& scene;
        // the gizmo lives only while the chrome is shown
        const bool& chromeShown;
        ClearColorSink clearColor;
        EditorPort port;
        EditorState state;
        // what was last applied, which a write is compared against
        EditorState applied;
        std::optional<usize> selection;
        Vec2 viewport{1.0f, 1.0f};
        InspectSections inspected;
        std::vector<Str> exposed;
        // raised when the selection changed, for panels to follow
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

        // applies `cut` and `key`; throws std::runtime_error when either is
        // not the content's
        void Start(StrView cut, StrView key);
        void Sync();
        // a frame's input already reached the camera
        void Update(Vec2 windowSize);
        // the window picks are measured in; one without area is ignored
        void SetViewport(Vec2 windowSize);

        void SelectCut(usize index);
        void SelectKey(usize index);
        void Select(std::optional<usize> object);
        // selects what the pixel shows, or nothing
        void PickAt(Vec2 pixel);
        // presses the gizmo at the pixel; whether a handle is held after it
        bool Grab(Vec2 pixel);
        // the held handle follows the pixel; `snap` is Ctrl
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
        GizmoHandle Held() const noexcept { return hold ? hold->drag.handle : GizmoHandle::None; }
        const GizmoView& Gizmo() const noexcept { return gizmoView; }
        // the selection's gizmo as the camera sees it now; nothing when it has
        // none, the chrome is hidden, or it would reach the near plane
        std::optional<GizmoLayout> SelectionGizmo() const;

    private:
        void exposeTargets();
        // the camera left its cut, by input or a write
        void leaveCut();
        bool applyCut();
        bool applyKey();
        bool applySelected();
        void applyPickAt();
        void applyGrab();
        void applyDrag();
        void applyHandle();
        void applyCancel();
        void release();
        // resolves the first section's position, yaw and scale
        void bindGizmo();
        void refreshGizmo();
        // a write into a section's row reaches the scene and the panel; the
        // `selection` exposures and the gizmo both make it
        void wroteSection(usize section);
        std::optional<usize> markerAt(Vec2 pixel) const;
        void refuse(Str status);
        void reportSelection();
        void exposeSelection();
        void unexposeSelection();
    };
}
