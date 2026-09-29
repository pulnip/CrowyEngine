#pragma once

// The engine's light loop pieces: a shader walks LightCount() lights
// through SampleLight and never sees the light buffer.

#include "SceneData.slang"

// mirror of LightKind in Engine/Render/Public/RenderLight.hpp
static const uint LightKindDirectional = 0;
static const uint LightKindPoint = 1;
static const uint LightKindSpot = 2;

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
    LightSample s = EvaluateLight(LoadLight(i), posWorld);
    s.attenuation *= s.shadow;

    return s;
}

// the hemisphere: ground colour facing down, sky colour facing up
float3 AmbientIrradiance(float3 N) {
    return lerp(groundAmbient.xyz, skyAmbient.xyz, N.y * 0.5 + 0.5);
}
