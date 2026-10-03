#include "StageContent.hpp"

#include <algorithm>
#include <format>
#include <limits>

#include "ClassRegistry.hpp"
#include "Geometry/Overlap3D.hpp"
#include "StringUtil.hpp"

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
        const StageGeometry& geometry,
        const StageTextureHandles& textures
    )
        : scene(scene),
          stage(stage),
          geometry(geometry),
          textures(textures) {
        build();
    }

    Color StageContent::ApplyKey(StrView key) {
        currentKey = stageKeyIndex(stage.document, key);

        return applyStageKey(scene, bindings, stage.document, currentKey);
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
            const auto& row = document.instances[object];
            const auto box = std::ranges::find(document.models, row.model, &StageModel::id)->box;

            return {InspectSection{
                .label = "transform",
                .target = &stage.document.instances[object],
                .desc = GetDesc<StageInstance>(),
                .apply = [this, object] { ApplyInstance(object); },
                // only the unit box is scaled per axis
                .gizmo = combine(GizmoParts::Move, GizmoParts::Turn, box ? GizmoParts::ScaleAxes : GizmoParts::Scale)
            }};
        }
        if(object < instances + quads) {
            const auto quad = object - instances;
            return {
                InspectSection{
                    .label = "transform",
                    .target = &stage.document.quads[quad],
                    .desc = GetDesc<StageQuad>(),
                    .apply = [this, quad] { ApplyQuad(quad); },
                    // width and height stay with the inspector
                    .gizmo = combine(GizmoParts::Move, GizmoParts::Turn)
                },
                InspectSection{
                    .label = "material",
                    .target = &scene.Materials().GetRef(bindings.quadMaterials[quad]).data,
                    .desc = GetDesc<MaterialData>(),
                    .apply = {}
                }
            };
        }

        const auto light = object - instances - quads;

        return {InspectSection{
            .label = "light",
            .target = &stage.document.lights[light],
            .desc = GetDesc<StageLight>(),
            .apply = [this, light] { ApplyLight(light); },
            .gizmo = GizmoParts::Move
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
        // at the time the scene shows, so an edit never resets a flipbook
        scene.Materials().GetRef(bindings.quadMaterials[quad]).data.uvScaleOffset =
            stageQuadRect(row, stage.sprites, sceneSeconds);
    }

    void StageContent::ApplyLight(usize light) {
        scene.Lights().GetRef(bindings.lights[light]) =
            stageLightSnapshot(stage.document.lights[light], stage.document.lightingKeys[currentKey]);
    }

    Str StageContent::ReadScene(StrView file) {
        auto path = toPath(Str(file).c_str());
        if(path.is_relative())
            path = stage.document.root / path;
        try {
            next = reloadStageDocument(stage, path);
        }
        catch(const std::exception& error) {
            next.reset();
            return error.what();
        }

        return {};
    }

    Str StageContent::SwapScene() {
        stage.document = std::move(next->document);
        stage.sprites = std::move(next->sprites);
        next.reset();
        scene.Clear();
        build();
        const auto& document = stage.document;

        return std::format(
            "{} instances, {} quads, {} lights, {} cuts, {} keys",
            document.instances.size(),
            document.quads.size(),
            document.lights.size(),
            cuts.size(),
            keys.size()
        );
    }

    bool StageContent::ApplyTime(f64 seconds) {
        sceneSeconds = seconds;
        auto changed = false;
        for(const auto quad: flipbooks) {
            const auto rect = stageQuadRect(stage.document.quads[quad], stage.sprites, seconds);
            auto& data = scene.Materials().GetRef(bindings.quadMaterials[quad]).data;
            if(data.uvScaleOffset != rect) {
                data.uvScaleOffset = rect;
                changed = true;
            }
        }

        return changed;
    }

    void StageContent::build() {
        bindings = populateStage(scene, stage, geometry, textures);
        cuts = editorCutsOf(stage);
        keys = keyNamesOf(stage.document);
        objects.clear();
        primitives.clear();
        lights.clear();
        meshes.clear();
        objectOfSlot.clear();
        currentKey = 0;
        // populate drew every flipbook's first frame
        sceneSeconds = 0.0;
        flipbooks.clear();
        for(usize i = 0; i < stage.document.quads.size(); ++i) {
            if(stage.document.quads[i].flipbook)
                flipbooks.push_back(i);
        }
        addObjects();
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
