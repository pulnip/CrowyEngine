#pragma once

#include <vector>

#include "EditorSession.hpp"
#include "RenderScene.hpp"
#include "StageScene.hpp"

namespace Crowy
{
    // the cut as the editor takes it
    EditorCut editorCutOf(const StageCut& cut);

    // The editor's view of the loaded stage: its cuts and lighting keys, its
    // instances, quads and lights as named objects grouped by area, and each
    // primitive's meshes for picking. Owns nothing the scene owns.
    class StageContent final: public EditorContent {
    private:
        using MeshLists = std::vector<std::vector<const MeshData*>>;

        RenderScene& scene;
        // the document's rows are what the inspector edits
        LoadedStage& stage;
        const StageBindings& bindings;
        std::vector<EditorCut> cuts;
        std::vector<Str> keys;
        std::vector<EditorObject> objects;
        // parallel to objects
        std::vector<std::optional<PrimitiveHandle>> primitives;
        std::vector<std::optional<LightHandle>> lights;
        MeshLists meshes;
        // by a primitive handle's slot
        std::vector<usize> objectOfSlot;

    public:
        StageContent(RenderScene& scene, LoadedStage& stage, const StageBindings& bindings);

        std::span<const EditorCut> Cuts() const override { return cuts; }
        std::span<const Str> Keys() const override { return keys; }
        Color ApplyKey(StrView key) override;

        EditorObjects Objects() const override { return objects; }
        std::optional<usize> ObjectOf(PrimitiveHandle primitive) const override;
        std::optional<PrimitiveHandle> PrimitiveOf(usize object) const override;
        std::optional<LightHandle> LightOf(usize object) const override;
        MeshList MeshesOf(PrimitiveHandle primitive) const override;
        InspectSections Inspect(usize object) override;

        // the instance row written into its primitive: matrix and bounds
        void ApplyInstance(usize instance);
        // the quad row written into its primitive and its material's rect
        void ApplyQuad(usize quad);

    private:
        void addObject(EditorObject object, std::optional<PrimitiveHandle> primitive, std::optional<LightHandle> light);
    };
}
