#pragma once

#include <vector>

#include "EditorSession.hpp"
#include "RenderScene.hpp"
#include "StageScene.hpp"

namespace Crowy
{
    // the cut as the editor takes it
    inline constexpr EditorCut editorCutOf(const StageCut& cut) {
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

    // The loaded stage as the editor sees it: cuts, keys, named objects by
    // area and each primitive's meshes to pick. Owns nothing the scene owns.
    class StageContent final: public EditorContent {
    private:
        using MeshLists = std::vector<std::vector<const MeshData*>>;
        using ObjectPrimitives = std::vector<std::optional<PrimitiveHandle>>;
        using ObjectLights = std::vector<std::optional<LightHandle>>;

        RenderScene& scene;
        // the document's rows are what the inspector edits
        LoadedStage& stage;
        const StageGeometry& geometry;
        const StageTextureHandles& textures;
        StageBindings bindings;
        // what ReadScene accepted, until SwapScene puts it in place
        std::optional<StageReload> next;
        std::vector<EditorCut> cuts;
        std::vector<Str> keys;
        std::vector<EditorObject> objects;
        // parallel to objects
        ObjectPrimitives primitives;
        ObjectLights lights;
        MeshLists meshes;
        // by a primitive handle's slot
        std::vector<usize> objectOfSlot;
        // the lighting key applied last, which a light's row is drawn under
        usize currentKey = 0;
        // the quads that play a flipbook, and the scene time they show
        std::vector<usize> flipbooks;
        f64 sceneSeconds = 0.0;

    public:
        StageContent(
            RenderScene& scene,
            LoadedStage& stage,
            const StageGeometry& geometry,
            const StageTextureHandles& textures
        );

        std::span<const EditorCut> Cuts() const override { return cuts; }
        std::span<const Str> Keys() const override { return keys; }
        Color ApplyKey(StrView key) override;

        EditorObjects Objects() const override { return objects; }
        std::optional<usize> ObjectOf(PrimitiveHandle primitive) const override;
        std::optional<PrimitiveHandle> PrimitiveOf(usize object) const override;
        std::optional<LightHandle> LightOf(usize object) const override;
        MeshList MeshesOf(PrimitiveHandle primitive) const override;
        InspectSections Inspect(usize object) override;
        Str ReadScene(StrView file) override;
        Str SwapScene() override;
        bool ApplyTime(f64 seconds) override;

        // the instance row written into its primitive: matrix and bounds
        void ApplyInstance(usize instance);
        // the quad row written into its primitive and its material's rect
        void ApplyQuad(usize quad);
        // the light row written into its snapshot under the current key
        void ApplyLight(usize light);

    private:
        // the rows and every table over them, from the stage as it stands
        void build();
        void addObjects();
        void addObject(EditorObject object, std::optional<PrimitiveHandle> primitive, std::optional<LightHandle> light);
    };
}
