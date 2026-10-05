#pragma once

// The view a pass binds at b1, mirrored from ViewData in
// RenderSceneData.hpp: what SceneData.slang and the effects both read.

// scalars and float4 only, so HLSL and Metal lay it out alike
cbuffer view : register(b1) {
    float4x4 viewProj;
    uint debugMode;
    // a ShadowFilter
    uint shadowFilter;
    uint _pad1;
    uint _pad2;
    // xyz = camera position, w unused
    float4 cameraPosition;
    // xyz, w unused; equal colours are a flat ambient
    float4 skyAmbient;
    float4 groundAmbient;
};
