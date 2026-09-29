#pragma once

// The engine's light loop pieces: a shader walks LightCount() lights
// through SampleLight and never sees the light buffer or the shadow map.

#include "ClipToTexture.slang"
#include "SceneData.slang"

// mirror of LightKind and NoShadow in Engine/Render/Public/RenderLight.hpp
static const uint LightKindDirectional = 0;
static const uint LightKindPoint = 1;
static const uint LightKindSpot = 2;
static const uint NoShadow = 0xFFFFFFFF;

// one light as a shading model sees it
struct LightSample {
    // unit, surface to light
    float3 direction;
    // colour x intensity: before attenuation, before any cosine
    float3 radiance;
    // falloff x shadow, >= 0, never thresholded by the engine
    float attenuation;
    // the shadow term alone: 0 or 1
    float shadow;
};

uint LightCount() {
    return pass.lightCount;
}

LightData LoadLight(uint i) {
    StructuredBuffer<LightData> lights = pass.lights;

    return lights[pass.lightBase + i];
}

// 1 where the light reaches posWorld, 0 where the map holds a nearer
// caster; one Load and one compare, no filter
float ShadowVisibility(LightData light, float3 posWorld, float3 geoNormal) {
    float3 L = -light.direction;
    // toward the light by a constant, along the normal by more as the
    // surface turns from the light
    float NoL = saturate(dot(geoNormal, L));
    float3 p = posWorld + L * light.shadowBias +
        geoNormal * (light.shadowNormalBias * (1.0 - NoL));

    // orthographic, so w is 1
    float4 clip = mul(light.worldToShadow, float4(p, 1.0));
    // ShadowSampleSpike pins ClipToTexture.slang's convention per backend
    float2 uv = ClipToTextureUV(clip.xy);
    if(any(uv < 0.0) || any(uv >= 1.0) || clip.z > 1.0)
        return 1.0;

    Texture2D<float> map = pass.shadowMap;
    uint width;
    uint height;
    map.GetDimensions(width, height);
    float stored = map.Load(int3(TextureUVToTexel(uv, uint2(width, height)), 0));

    return clip.z <= stored ? 1.0 : 0.0;
}

// direction, radiance and falloff, before any shadow
LightSample EvaluateLight(LightData light, float3 posWorld) {
    LightSample s;
    s.radiance = light.color;
    s.shadow = 1.0;
    s.attenuation = 1.0;
    if(light.kind == LightKindDirectional) {
        s.direction = -light.direction;

        return s;
    }

    float3 toLight = light.position - posWorld;
    float d2 = max(dot(toLight, toLight), 1e-4);
    s.direction = toLight * rsqrt(d2);
    // inverse square, windowed to 0 at the range
    float x = d2 * light.invRange * light.invRange;
    float window = saturate(1.0 - x * x);
    // glTF's cone; a point light's (0, 1) leaves 1
    float cone = saturate(
        dot(light.direction, -s.direction) * light.coneScale + light.coneOffset
    );
    s.attenuation = window * window / d2 * cone * cone;

    return s;
}

// geoNormal is the surface's geometric normal, which a shadow lookup
// offsets along
LightSample SampleLight(uint i, float3 posWorld, float3 geoNormal) {
    LightData light = LoadLight(i);
    LightSample s = EvaluateLight(light, posWorld);
    // an if, not ?:, so an unshadowed light never reads the map
    if(light.shadowIndex != NoShadow)
        s.shadow = ShadowVisibility(light, posWorld, geoNormal);
    s.attenuation *= s.shadow;

    return s;
}

// every light's shadow term multiplied: a second walk, for a debug view
float ShadowTerm(float3 posWorld, float3 geoNormal) {
    float shadow = 1.0;
    for(uint i = 0; i < LightCount(); ++i)
        shadow *= SampleLight(i, posWorld, geoNormal).shadow;

    return shadow;
}

// the hemisphere: ground colour facing down, sky colour facing up
float3 AmbientIrradiance(float3 N) {
    return lerp(groundAmbient.xyz, skyAmbient.xyz, N.y * 0.5 + 0.5);
}
