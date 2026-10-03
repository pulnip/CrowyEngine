#include "StageContent.hpp"

namespace Crowy
{
    EditorCut editorCutOf(const StageCut& cut) {
        return EditorCut{
            .name = cut.name,
            .position = cut.position,
            .yaw = cut.yaw,
            .pitch = cut.pitch,
            .lens = {
                .fovY = cut.fovY,
                .nearZ = cut.nearZ,
                .farZ = cut.farZ,
                .orthographic = cut.orthographic,
                .orthoHalfHeight = cut.orthoHalfHeight
            }
        };
    }

    StageContent::StageContent(
        RenderScene& scene,
        const LoadedStage& stage,
        const StageBindings& bindings
    )
        : scene(scene),
          stage(stage),
          bindings(bindings) {
        for(const auto& cut: makeStageCuts(stage))
            cuts.push_back(editorCutOf(cut));
        for(const auto& key: stage.document.lightingKeys)
            keys.push_back(key.name);
    }

    Color StageContent::ApplyKey(StrView key) {
        return applyStageKey(scene, bindings, stage.document, stageKeyIndex(stage.document, key));
    }
}
