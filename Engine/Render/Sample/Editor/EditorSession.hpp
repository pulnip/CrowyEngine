#pragma once

#include <functional>
#include <span>

#include "EditorCamera.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class EditorContent;
    class EditorSession;

    using ClearColorSink = std::function<void(Color)>;

    // The editor's state as one reflected target, `editor`: the panel, the
    // keyboard and the port all write a field and call Sync, which applies
    // what changed and reverts what it cannot apply.
    struct EditorState {
        // a cut's name, or "free" once the camera flies
        Str cut;
        // a lighting key's name
        Str key;
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
    };

    class EditorSession {
    public:
        // the state the camera reports once it has flown off a cut
        static constexpr CStr FreeCut = "free";

    private:
        EditorCamera& camera;
        EditorContent& content;
        ClearColorSink clearColor;
        EditorState state;
        // what was last applied, which a write is compared against
        EditorState applied;

    public:
        EditorSession(EditorCamera& camera, EditorContent& content, ClearColorSink clearColor);

        // applies `cut` and `key`; throws std::runtime_error when either is
        // not the content's
        void Start(StrView cut, StrView key);
        void Sync();
        // a frame's input already reached the camera
        void Update();

        void SelectCut(usize index);
        void SelectKey(usize index);

        EditorState& State() noexcept { return state; }
        const EditorState& State() const noexcept { return state; }

    private:
        bool applyCut();
        bool applyKey();
    };
}
