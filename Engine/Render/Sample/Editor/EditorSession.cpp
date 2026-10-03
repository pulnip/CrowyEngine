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
        // "a, b, c"
        template<typename R, typename F>
        Str listOf(const R& items, F&& name) {
            Str list;
            for(const auto& item: items)
                list += list.empty() ? Str(name(item)) : ", " + Str(name(item));

            return list;
        }
    }

    EditorSession::EditorSession(
        EditorCamera& camera,
        EditorContent& content,
        ClearColorSink clearColor
    )
        : camera(camera),
          content(content),
          clearColor(std::move(clearColor)) {}

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
        applied = state;
    }

    void EditorSession::Update() {
        if(camera.TakeMoved() && state.cut != FreeCut) {
            state.cut = FreeCut;
            applied.cut = FreeCut;
        }
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

    bool EditorSession::applyCut() {
        if(state.cut == FreeCut)
            return true;

        const auto cuts = content.Cuts();
        const auto found = std::ranges::find(cuts, state.cut, &EditorCut::name);
        if(found == cuts.end()) {
            state.status = std::format(
                "no cut '{}'; there are {}, or {}",
                state.cut,
                listOf(cuts, [](const EditorCut& cut) { return cut.name; }),
                FreeCut
            );
            applied.status = state.status;
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
            state.status = std::format(
                "no key '{}'; there are {}",
                state.key,
                listOf(keys, [](const Str& key) { return key; })
            );
            applied.status = state.status;
            return false;
        }

        clearColor(content.ApplyKey(state.key));
        state.status = std::format("key {}", state.key);
        applied.status = state.status;

        return true;
    }

    // clang-format off
    CROWY_STRUCT(EditorState)
        .SetProperty("cut", &EditorState::cut)
        .SetProperty("key", &EditorState::key)
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
