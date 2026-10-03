#pragma once

#include <functional>
#include <optional>
#include <span>

#include <vector>

#include "ClassRegistry.hpp"
#include "EditorCamera.hpp"
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
    using MeshList = std::span<const MeshData* const>;

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
    // into it into scene rows
    struct InspectSection {
        Str label;
        void* target = nullptr;
        const TypeDesc* desc = nullptr;
        DirtyCallback apply;
    };

    // the port as the session uses it, empty where there is none
    struct EditorPort {
        std::function<void(StrView, void*, const TypeDesc&, DirtyCallback)> expose;
        std::function<void(StrView)> unexpose;
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
        // a pixel to pick at, window points from the top-left
        Vec2 pickAt{-1.0f, -1.0f};
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
        // the port's names for the state and the camera
        static constexpr CStr StateTarget = "editor";
        static constexpr CStr CameraTarget = "camera";
        // the port's name for the selection's first section; the others are
        // "selection.<label>"
        static constexpr CStr SelectionTarget = "selection";

    private:
        EditorCamera& camera;
        EditorContent& content;
        const RenderScene& scene;
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
        // raised when a port write changed what the inspector shows
        bool inspectorDirty = false;

    public:
        EditorSession(
            EditorCamera& camera,
            EditorContent& content,
            const RenderScene& scene,
            ClearColorSink clearColor,
            EditorPort port = {}
        );
        ~EditorSession();
        CROWY_DECLARE_PINNED(EditorSession)

        // applies `cut` and `key`; throws std::runtime_error when either is
        // not the content's
        void Start(StrView cut, StrView key);
        void Sync();
        // a frame's input already reached the camera
        void Update(Vec2 windowSize);

        void SelectCut(usize index);
        void SelectKey(usize index);
        void Select(std::optional<usize> object);
        // selects what the pixel shows, or nothing
        void PickAt(Vec2 pixel);

        EditorState& State() noexcept { return state; }
        const EditorState& State() const noexcept { return state; }
        std::optional<usize> Selection() const noexcept { return selection; }
        const EditorContent& Content() const noexcept { return content; }
        const EditorCamera& Camera() const noexcept { return camera; }
        Vec2 Viewport() const noexcept { return viewport; }
        bool TakeSelectionChanged() noexcept;
        bool TakeInspectorDirty() noexcept;
        const InspectSections& Inspected() const noexcept { return inspected; }

    private:
        void exposeTargets();
        // the camera left its cut, by input or a write
        void leaveCut();
        bool applyCut();
        bool applyKey();
        bool applySelected();
        void applyPickAt();
        void refuse(Str status);
        void reportSelection();
        void exposeSelection();
        void unexposeSelection();
    };
}
