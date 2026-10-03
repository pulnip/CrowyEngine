#pragma once

#include <functional>
#include <optional>
#include <span>

#include "EditorCamera.hpp"
#include "EditorPick.hpp"
#include "Primitives.hpp"
#include "RenderScene.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class EditorContent;
    class EditorSession;
    struct EditorObject;

    using ClearColorSink = std::function<void(Color)>;
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

    // The editor's state as one reflected target, `editor`: the panel, the
    // keyboard and the port all write a field and call Sync, which applies
    // what changed and reverts what it cannot apply.
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
    };

    class EditorSession {
    public:
        // the state the camera reports once it has flown off a cut
        static constexpr CStr FreeCut = "free";

    private:
        EditorCamera& camera;
        EditorContent& content;
        const RenderScene& scene;
        ClearColorSink clearColor;
        EditorState state;
        // what was last applied, which a write is compared against
        EditorState applied;
        std::optional<usize> selection;
        Vec2 viewport{1.0f, 1.0f};
        // raised when the selection changed, for panels to follow
        bool selectionChanged = false;

    public:
        EditorSession(
            EditorCamera& camera,
            EditorContent& content,
            const RenderScene& scene,
            ClearColorSink clearColor
        );

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

    private:
        bool applyCut();
        bool applyKey();
        bool applySelected();
        void applyPickAt();
        void refuse(Str status);
        void reportSelection();
    };
}
