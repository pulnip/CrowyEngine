#include "StageContent.hpp"

#include <algorithm>
#include <format>
#include <limits>

#include "ClassRegistry.hpp"
#include "Geometry/Overlap3D.hpp"

namespace Crowy
{
    namespace
    {
        constexpr usize NoObject = std::numeric_limits<usize>::max();

        std::vector<EditorCut> editorCutsOf(const LoadedStage& stage) {
            std::vector<EditorCut> cuts;
            for(const auto& cut: makeStageCuts(stage))
                cuts.push_back(editorCutOf(cut));

            return cuts;
        }

        std::vector<Str> keyNamesOf(const StageDocument& document) {
            std::vector<Str> keys;
            for(const auto& key: document.lightingKeys)
                keys.push_back(key.name);

            return keys;
        }
    }

    StageContent::StageContent(
        RenderScene& scene,
        LoadedStage& stage,
        const StageBindings& bindings
    )
        : scene(scene),
          stage(stage),
          bindings(bindings),
          cuts(editorCutsOf(stage)),
          keys(keyNamesOf(stage.document)) {
        addObjects();
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

    InspectSections StageContent::Inspect(usize object) {
        const auto& document = stage.document;
        const auto instances = document.instances.size();
        const auto quads = document.quads.size();

        if(object < instances) {
            return {InspectSection{
                .label = "transform",
                .target = &stage.document.instances[object],
                .desc = GetDesc<StageInstance>(),
                .apply = [this, object] { ApplyInstance(object); }
            }};
        }
        if(object < instances + quads) {
            const auto quad = object - instances;
            return {
                InspectSection{
                    .label = "transform",
                    .target = &stage.document.quads[quad],
                    .desc = GetDesc<StageQuad>(),
                    .apply = [this, quad] { ApplyQuad(quad); }
                },
                InspectSection{
                    .label = "material",
                    .target = &scene.Materials().GetRef(bindings.quadMaterials[quad]).data,
                    .desc = GetDesc<MaterialData>(),
                    .apply = {}
                }
            };
        }

        return {InspectSection{
            .label = "light",
            .target = &scene.Lights().GetRef(*lights[object]),
            .desc = GetDesc<LightSnapshot>(),
            .apply = {}
        }};
    }

    void StageContent::ApplyInstance(usize instance) {
        const auto& row = stage.document.instances[instance];
        const auto model = std::ranges::find(stage.document.models, row.model, &StageModel::id) - stage.document.models.begin();
        auto& primitive = scene.Primitives().GetRef(bindings.instances[instance]);
        primitive.localToWorld = instanceToWorld(row);
        primitive.worldBounds = transformAABB3D(primitive.localToWorld, stage.models[static_cast<usize>(model)].bounds);
    }

    void StageContent::ApplyQuad(usize quad) {
        const auto& row = stage.document.quads[quad];
        auto& primitive = scene.Primitives().GetRef(bindings.quads[quad]);
        primitive.localToWorld = quadToWorld(row);
        primitive.worldBounds =
            transformAABB3D(primitive.localToWorld, scene.Meshes().GetRef(primitive.mesh).localBounds);
        scene.Materials().GetRef(bindings.quadMaterials[quad]).data.uvScaleOffset = Vec4{
            row.uv1.x - row.uv0.x,
            row.uv1.y - row.uv0.y,
            row.uv0.x,
            row.uv0.y
        };
    }

    void StageContent::addObjects() {
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
