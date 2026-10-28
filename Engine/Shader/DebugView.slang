#pragma once

// A view uniform branch rather than a material swap, as UE's
// UnlitViewmodeMask does: a swap would split VS and FS across files.

#include "RenderEnums.h"

// Additive saturation turns layer counts into a heat ramp: red saturates at
// 5 layers, green at 10, blue at 20.
static const float3 OverdrawLayer = float3(0.2, 0.1, 0.05);

// called last; lit and unshaded are scene radiance the post list tone maps,
// the other views data copied out untouched; shadow is 1 lit, 0 shadowed
float3 applyDebugView(
    uint mode,
    float3 lit,
    float3 unshaded,
    float3 normalWS,
    float deviceDepth,
    float shadow
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
    case DebugModeShadow:
        return shadow.xxx;
    default:
        return lit;
    }
}
