#pragma once

#include <algorithm>
#include <cmath>
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

    // the atlas rect a quad's material samples at `seconds`, as uvScaleOffset:
    // the row's rect, moved by whole cells to its flipbook's frame
    inline Vec4 stageQuadRect(const StageQuad& quad, const StageSprites& sprites, f64 seconds) {
        auto offset = quad.uv0;
        if(quad.flipbook) {
            const auto& sprite = sprites.at(quad.flipbook->sprite);
            const auto& animation =
                *std::ranges::find(sprite.animations, quad.flipbook->animation, &StageSpriteAnimation::name);
            // whole microseconds, rounded: a seek to n frame lengths lands on frame n
            const auto micros = std::isfinite(seconds) && seconds > 0.0
                ? static_cast<u64>(std::min(seconds, 1.0e9) * 1.0e6 + 0.5)
                : u64{0};
            const auto frame = micros / (u64{animation.frameDurationMs} * 1000) % animation.frameCount;
            const auto cell = animation.startRow * sprite.columns + animation.startColumn + frame;
            const auto columns = static_cast<f32>(sprite.columns);
            const auto rows = static_cast<f32>(sprite.rows);
            offset.x += (static_cast<f32>(cell % sprite.columns) - static_cast<f32>(animation.startColumn)) / columns;
            offset.y += (static_cast<f32>(cell / sprite.columns) - static_cast<f32>(animation.startRow)) / rows;
        }

        return Vec4{quad.uv1.x - quad.uv0.x, quad.uv1.y - quad.uv0.y, offset.x, offset.y};
    }

    // a light row as the scene draws it under `key`: off where its group's
    // scale is 0
    inline LightSnapshot stageLightSnapshot(const StageLight& light, const StageLightingKey& key) {
        const auto group = std::ranges::find(key.lightGroups, light.group, &StageScale::name);
        const auto scale = group == key.lightGroups.end() ? 0.0f : group->scale;

        return LightSnapshot{
            .kind = light.kind == StageLightKind::Spot ? LightKind::Spot : LightKind::Point,
            .enabled = scale > 0.0f,
            .color = light.color,
            .intensity = light.intensity * scale,
            .position = light.position,
            .direction = normSquared(light.direction) > 0.0f ? normalize(light.direction) : -unitY(),
            .range = light.range,
            .innerConeAngle = static_cast<f32>(toRadian(light.innerAngle)),
            .outerConeAngle = static_cast<f32>(toRadian(light.outerAngle))
        };
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
