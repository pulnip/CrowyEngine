#pragma once

#include <map>
#include <vector>

#include "GeometryPool.hpp"
#include "LinearAlgebra.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "StageLoad.hpp"

// The loaded stage as RenderScene rows: every Backlot rule the engine draws
// with lives here, never under Engine/.
namespace Crowy
{
    struct StageBindings;
    struct StageCut;
    struct StageGeometry;

    using GeometryAllocations = std::vector<GeometryAllocation>;
    using StageCuts = std::vector<StageCut>;
    using StageEmissivePalettes = std::map<Str, MaterialHandle>;
    using StageLightHandles = std::vector<LightHandle>;
    using StageMaterialHandles = std::vector<MaterialHandle>;
    using StageModelAllocations = std::vector<GeometryAllocations>;
    using StagePrimitiveHandles = std::vector<PrimitiveHandle>;
    using StageTextureHandles = std::map<Str, TextureHandle>;

    // Unity's preview draws every surface at smoothness 0.1
    inline constexpr f32 StageRoughness = 0.9f;

    // scale, then yaw, then translation, as the contract orders them
    inline Mat4 instanceToWorld(const StageInstance& instance) {
        return translateMat(instance.position)
            * rotateYMat(static_cast<f32>(toRadian(instance.yaw)))
            * scaleMat(instance.scale);
    }

    // a unit quad facing +Z stretched to width x height, then yawed and moved
    inline Mat4 quadToWorld(const StageQuad& quad) {
        return translateMat(quad.position)
            * rotateYMat(static_cast<f32>(toRadian(quad.yaw)))
            * scaleMat({quad.width, quad.height, 1.0f});
    }

    // queues every model slot and the unit quad; call from OnBuildGeometry
    StageGeometry addStageGeometry(GeometryPool& pool, const LoadedStage& stage);
    // uploads every image once; the scene owns them, and a reload keeps them
    StageTextureHandles uploadStageTextures(
        RenderScene& scene,
        RHIDevice& device,
        const LoadedStage& stage
    );
    // the rows: materials, meshes, primitives, lights and the sun, all off
    // until a key is applied
    StageBindings populateStage(
        RenderScene& scene,
        const LoadedStage& stage,
        const StageGeometry& geometry,
        const StageTextureHandles& textures
    );
    // a lighting key, switched: the sun, the ambient pair, light groups,
    // emissive channels and keyed rows; returns the key's clear color
    Color applyStageKey(
        RenderScene& scene,
        const StageBindings& bindings,
        const StageDocument& document,
        usize key
    );
    // throws std::runtime_error naming the keys there are
    usize stageKeyIndex(const StageDocument& document, StrView name);
    // the scene file's cameras in the engine's units and pitch sign, each
    // with a near plane fitted to what stands closest to it
    StageCuts makeStageCuts(const LoadedStage& stage);

    struct StageGeometry {
        // parallel to the document's models, one per model slot
        StageModelAllocations models;
        GeometryAllocation unitQuad{};
    };

    // every row extraction added, by document row
    struct StageBindings {
        MaterialHandle palette;
        // by emissive channel
        StageEmissivePalettes emissivePalettes;
        StagePrimitiveHandles instances;
        StagePrimitiveHandles quads;
        StageMaterialHandles quadMaterials;
        StageLightHandles lights;
        LightHandle sun;
    };

    struct StageCut {
        Str name;
        Vec3 position{};
        // radians, the engine's signs: yaw clockwise from +Z, pitch down
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        f32 fovY = 1.0f;
        bool orthographic = false;
        f32 orthoHalfHeight = 0.0f;
        f32 nearZ = 0.3f;
        f32 farZ = 1000.0f;
    };
}
