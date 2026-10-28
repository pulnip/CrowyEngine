#pragma once

// The view a pass binds at b1, ViewData on the C++ side: what SceneData.slang
// and the effects both read.

// scalars and float4 only, so HLSL and Metal lay it out alike
cbuffer view : register(b1) {
    float4x4 viewProj;
    uint debugMode;
    // a ShadowFilter
    uint shadowFilter;
    uint _pad1;
    uint _pad2;
    float4 cameraPosition;
    // equal colors are a flat ambient
    float4 skyAmbient;
    float4 groundAmbient;
};
