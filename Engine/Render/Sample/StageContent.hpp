#pragma once

#include <vector>

#include "EditorSession.hpp"
#include "RenderScene.hpp"
#include "StageScene.hpp"

namespace Crowy
{
    // the cut as the editor takes it
    EditorCut editorCutOf(const StageCut& cut);

    // The editor's view of the loaded stage: its cuts, its lighting keys,
    // and a key's rows. Owns nothing the scene owns.
    class StageContent final: public EditorContent {
    private:
        RenderScene& scene;
        const LoadedStage& stage;
        const StageBindings& bindings;
        std::vector<EditorCut> cuts;
        std::vector<Str> keys;

    public:
        StageContent(RenderScene& scene, const LoadedStage& stage, const StageBindings& bindings);

        std::span<const EditorCut> Cuts() const override { return cuts; }
        std::span<const Str> Keys() const override { return keys; }
        Color ApplyKey(StrView key) override;
    };
}
