#pragma once

// A view uniform branch rather than a material swap, as UE's
// UnlitViewmodeMask does: a swap would split VS and FS across files.

// mirror of DebugMode in Engine/Render/Sample/RenderApp.hpp
static const uint DebugModeLit = 0;
static const uint DebugModeUnshaded = 1;
static const uint DebugModeNormals = 2;
static const uint DebugModeDepth = 3;
static const uint DebugModeOverdraw = 4;

// Additive saturation turns layer counts into a heat ramp: red saturates at
// 5 layers, green at 10, blue at 20.
static const float3 OverdrawLayer = float3(0.2, 0.1, 0.05);

// Called last in a fragment shader. lit and unshaded come display-ready from
// the caller; the other views are data and skip its display transform.
float3 ApplyDebugView(
    uint mode,
    float3 lit,
    float3 unshaded,
    float3 normalWS,
    float deviceDepth
){
    switch(mode) {
    case DebugModeUnshaded:
        return unshaded;
    case DebugModeNormals:
        return normalWS * 0.5 + 0.5;
    case DebugModeDepth:
        // standard 0..1 depth crowds far from the camera; tuned to a near
        // plane around 0.05 until views carry their own planes
        return (1.0 - pow(deviceDepth, 100.0)).xxx;
    case DebugModeOverdraw:
        return OverdrawLayer;
    default:
        return lit;
    }
}
