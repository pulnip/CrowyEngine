#include "StageContent.hpp"

#include <algorithm>
#include <format>
#include <limits>

namespace Crowy
{
    namespace
    {
        constexpr usize NoObject = std::numeric_limits<usize>::max();
    }

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

        const auto& document = stage.document;
        for(usize i = 0; i < document.instances.size(); ++i) {
            const auto& instance = document.instances[i];
            addObject(
                EditorObject{
                    .name = std::format("instance/{}", instance.name),
                    .group = instance.area,
                    .detail = instance.model,
                    .kind = EditorObjectKind::Instance
                },
                bindings.instances[i],
                std::nullopt
            );
            const auto model = std::ranges::find(document.models, instance.model, &StageModel::id) - document.models.begin();
            auto& list = meshes.back();
            for(const auto& slot: stage.models[static_cast<usize>(model)].slots)
                list.push_back(&slot.mesh);
        }
        for(usize i = 0; i < document.quads.size(); ++i) {
            const auto& quad = document.quads[i];
            addObject(
                EditorObject{
                    .name = std::format("quad/{}", quad.name),
                    .group = quad.area,
                    .detail = std::format("{} {}", quad.material, quad.image),
                    .kind = EditorObjectKind::Quad
                },
                bindings.quads[i],
                std::nullopt
            );
            meshes.back().push_back(&stage.unitQuad);
        }
        for(usize i = 0; i < document.lights.size(); ++i) {
            const auto& light = document.lights[i];
            addObject(
                EditorObject{
                    .name = std::format("light/{}", light.name),
                    .group = std::format("Lights · {}", light.group),
                    .detail = light.kind == StageLightKind::Spot ? "spot" : "point",
                    .kind = EditorObjectKind::Light
                },
                std::nullopt,
                bindings.lights[i]
            );
        }
    }

    Color StageContent::ApplyKey(StrView key) {
        return applyStageKey(scene, bindings, stage.document, stageKeyIndex(stage.document, key));
    }

    std::optional<usize> StageContent::ObjectOf(PrimitiveHandle primitive) const {
        const auto slot = primitive.GetIndex();
        if(slot >= objectOfSlot.size() || objectOfSlot[slot] == NoObject)
            return std::nullopt;

        const auto object = objectOfSlot[slot];
        if(primitives[object] != primitive)
            return std::nullopt;

        return object;
    }

    std::optional<PrimitiveHandle> StageContent::PrimitiveOf(usize object) const {
        return object < primitives.size() ? primitives[object] : std::nullopt;
    }

    std::optional<LightHandle> StageContent::LightOf(usize object) const {
        return object < lights.size() ? lights[object] : std::nullopt;
    }

    MeshList StageContent::MeshesOf(PrimitiveHandle primitive) const {
        const auto object = ObjectOf(primitive);

        return object ? MeshList(meshes[*object]) : MeshList{};
    }

    void StageContent::addObject(
        EditorObject object,
        std::optional<PrimitiveHandle> primitive,
        std::optional<LightHandle> light
    ) {
        if(primitive) {
            const auto slot = primitive->GetIndex();
            if(slot >= objectOfSlot.size())
                objectOfSlot.resize(slot + 1, NoObject);
            objectOfSlot[slot] = objects.size();
        }
        objects.push_back(std::move(object));
        primitives.push_back(primitive);
        lights.push_back(light);
        meshes.emplace_back();
    }
}
