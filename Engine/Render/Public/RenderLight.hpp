#pragma once

#include <cstddef>
#include <type_traits>

#include "EnumUtil.hpp"
#include "LinearAlgebra.hpp"
#include "PackedTable.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct LightSnapshot;

    using LightHandle = GenericHandle<LightSnapshot>;
    using LightTable = PackedTable<LightSnapshot>;

    // no shadow-map slot: the shader skips its lookup
    inline constexpr u32 NoShadow = ~0u;

    // mirrored by the constants in Engine/Shader/Lighting.slang
    enum class LightKind : u32 {
        Directional,
        Point,
        Spot,
    };

    CROWY_ENUM_BEGIN(LightKind)
        CROWY_ENUM_VALUE(Directional)
        CROWY_ENUM_VALUE(Point)
        CROWY_ENUM_VALUE(Spot)
    CROWY_ENUM_END()

    // how a receiver reads the shadow map; mirrored by the constants in
    // Engine/Shader/Lighting.slang
    enum class ShadowFilter : u32 {
        // one texel, one compare: exactly 0 or 1
        Hard,
        // four compares weighted bilinearly: a soft edge a texel wide
        Pcf2x2,
    };

    CROWY_ENUM_BEGIN(ShadowFilter)
        CROWY_ENUM_VALUE(Hard)
        CROWY_ENUM_VALUE(Pcf2x2)
    CROWY_ENUM_END()

    // What extraction writes; packed into a LightData row each frame.
    struct LightSnapshot {
        LightKind kind = LightKind::Directional;
        bool enabled = true;
        bool castShadow = false;
        Vec3 color = ones();
        f32 intensity = 1.0f;
        Vec3 position = zeros();
        // the way the light travels
        Vec3 direction = -unitY();
        // metres to zero; point, spot
        f32 range = 10.0f;
        // half-angles, radians
        f32 innerConeAngle = 0.0f;
        f32 outerConeAngle = 0.785f;
        // metres toward the light
        f32 shadowBias = 0.02f;
        // shadow-map texels along the normal
        f32 shadowNormalBias = 1.0f;
    };

    // equal colours are a flat ambient
    struct EnvironmentSnapshot {
        Vec3 skyAmbient = zeros();
        Vec3 groundAmbient = zeros();
    };

    // One light as the shaders read it, mirrored in Engine/Shader/SceneData.slang.
    // Each Vec3 is followed by a 4-byte scalar, as in MaterialData.
    struct LightData {
        // world to the shadow view's clip space; identity when unshadowed
        Mat4 worldToShadow = unitMat();
        Vec3 position = zeros();
        // 0 for directional
        f32 invRange = 0.0f;
        // unit, the way the light travels
        Vec3 direction = -unitY();
        LightKind kind = LightKind::Directional;
        // colour x intensity
        Vec3 color = zeros();
        u32 shadowIndex = NoShadow;
        // glTF's cone as scale and offset on cos; (0, 1) leaves every
        // direction lit
        f32 coneScale = 0.0f;
        f32 coneOffset = 1.0f;
        // metres toward the light
        f32 shadowBias = 0.0f;
        // metres along the normal
        f32 shadowNormalBias = 0.0f;
    };
    static_assert(sizeof(LightData) == 128);
    static_assert(offsetof(LightData, position) == 64);
    static_assert(offsetof(LightData, direction) == 80);
    static_assert(offsetof(LightData, kind) == 92);
    static_assert(offsetof(LightData, color) == 96);
    static_assert(offsetof(LightData, shadowIndex) == 108);
    static_assert(offsetof(LightData, coneScale) == 112);
    static_assert(offsetof(LightData, shadowBias) == 120);
    static_assert(std::is_trivially_copyable_v<LightData>);
}
