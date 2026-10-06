#pragma once

// What the surface and panel passes share: the paint buffer's decoding, the
// atlas lookup MF_PaintOverlay does, and colors for the data views.

#include "PaintShared.h"
#include "SRGB.slang"

// the low three bits of R's byte
uint DecodePaintId(float r) {
    return uint(round(r * 255.0)) & 7u;
}

uint DecodePaintStarGen(float r) {
    return uint(round(r * 255.0)) >> 3u;
}

// the dominant axis of a mesh-space normal, ties X over Y over Z, as
// MF_PaintOverlay picks it: 0 Front, 1 Back, 2 Right, 3 Left, 4 Up, 5 Down
uint IslandDirection(float3 N) {
    float3 A = abs(N);
    if(A.x >= A.y && A.x >= A.z)
        return N.x >= 0.0 ? 0u : 1u;
    if(A.y >= A.z)
        return N.y >= 0.0 ? 2u : 3u;

    return N.z >= 0.0 ? 4u : 5u;
}

// MF_PaintOverlay's Custom_0: `n` is the position over the mesh bounds; a
// direction not kept reads the first gutter texel, which never holds paint
float2 IslandUV(float3 n, float4 R, uint direction, float TexelSize) {
    float2 P = direction < 2u ? n.yz : (direction < 4u ? n.xz : n.xy);
    if(R.z <= 0.0)
        return float2(0.5, 0.5) * TexelSize;

    return R.xy + P * R.zw;
}

// the radiance a hook writes so Reinhard and sRGB land it on `display`;
// capped below 1, where L / (1 - L) is infinite
float3 DisplayToRadiance(float3 display) {
    float3 L = min(srgb2linear(saturate(display)), 0.99998);

    return L / (1.0 - L);
}

// the six directions' colors, in enum order
float3 DirectionColor(uint direction) {
    static const float3 colors[6] = {
        float3(0.93, 0.42, 0.36),
        float3(0.62, 0.24, 0.20),
        float3(0.40, 0.72, 0.38),
        float3(0.20, 0.44, 0.22),
        float3(0.38, 0.58, 0.94),
        float3(0.20, 0.30, 0.56)
    };

    return colors[min(direction, 5u)];
}

// MPC_TeamLook's colors as display values: 0 and 2 Mint, 1 and 3 Choco
float3 TeamDisplayColor(uint id) {
    if(id == PAINT_ID_NONE)
        return float3(0.86, 0.84, 0.82);
    if(id >= 4u)
        return float3(0.55, 0.55, 0.55);

    return (id & 1u) == 0u ? float3(0.12, 0.66, 0.56) : float3(0.42, 0.27, 0.19);
}

// a dark-to-mint ramp for the 0..1 channels
float3 RampColor(float t) {
    t = saturate(t);
    float3 low = float3(0.08, 0.07, 0.07);
    float3 mid = float3(0.12, 0.66, 0.56);
    float3 high = float3(0.95, 0.98, 0.96);

    return t < 0.5 ? lerp(low, mid, t * 2.0) : lerp(mid, high, t * 2.0 - 1.0);
}
