#pragma once

// Shared by every pass that draws into the orbit view. The radius warp and the
// polyline expansion live here rather than in one shader and a copy, because
// the grid only tells the truth if it went through exactly the same warp the
// trails did - a copy that drifted would turn the rings into a lie.

// keep in sync with ORBIT_BODY_COUNT / ORBIT_EARTH_INDEX in Kepler.hpp
static const uint BODY_COUNT = 9;
static const uint EARTH_INDEX = 3;

static const float ORBIT_PI = 3.14159265358979324;

float orbitWrapToPi(float radians){
    return radians - 6.28318530717958648 *
        floor((radians + ORBIT_PI) * 0.159154943091895336);
}

// Interpolate first, warp second. The other order does the subtraction in a
// nonlinear space and the geometry comes apart.
//
// Because the warp runs afterwards its centre is always the current frame's
// origin, so it slides from an Earth-centred log to a Sun-centred one as alpha
// moves without being told anything about alpha.
//
// Positions are already in AU, so the AU in log(1 + r/AU) is 1. Linear, 30 AU
// against 0.39 AU is 77:1 and Mercury is a pixel; through the log it is 10:1
// and every planet is on screen at once.
float3 orbitLogWarp(float3 pLin){
    const float r = length(pLin);

    return r > 1e-6 ? pLin * (log(1.0 + r) / r) : float3(0.0, 0.0, 0.0);
}

// One corner of a screen-space thickened segment, as a triangle strip:
//
//   vertexID  0        1        2        3
//             (p0,-1)  (p0,+1)  (p1,-1)  (p1,+1)
//
// The perpendicular is taken in pixels, not in NDC. NDC units are not square
// on a non-square viewport, so a perpendicular computed there shears every line
// that is not axis-aligned.
float4 orbitExpandSegment(
    float4 c0,
    float4 c1,
    uint vertexID,
    float2 viewport,
    float thicknessPx
){
    const float2 ndc0 = c0.xy / c0.w;
    const float2 ndc1 = c1.xy / c1.w;

    const uint endpoint = vertexID >> 1;
    const float side = (vertexID & 1) != 0 ? 1.0 : -1.0;

    const float2 halfViewport = viewport * 0.5;

    const float2 deltaPx = (ndc1 - ndc0) * halfViewport;
    const float lengthPx = length(deltaPx);
    // a zero-length segment has no direction to be perpendicular to; the Sun's
    // trail is entirely made of them at alpha = 1
    const float2 dirPx = lengthPx > 1e-6 ?
        deltaPx / lengthPx :
        float2(1.0, 0.0);
    const float2 perpPx = float2(-dirPx.y, dirPx.x) * (thicknessPx * 0.5) * side;

    const float4 clip = endpoint == 0 ? c0 : c1;
    const float2 ndc = (endpoint == 0 ? ndc0 : ndc1) + perpPx / halfViewport;

    // back through the perspective divide the rasterizer is about to redo.
    // The camera is orthographic here, so w is 1 and this is a no-op - it is
    // written out anyway so an off-ecliptic camera later costs nothing.
    return float4(ndc * clip.w, clip.z, clip.w);
}

// A screen-space square centred on one point, for markers.
// Same strip corner order as above; uv comes back in [-1, 1] so the fragment
// stage can cut a disc out of it.
float4 orbitExpandPoint(
    float4 clip,
    uint vertexID,
    float2 viewport,
    float radiusPx,
    out float2 uv
){
    uv = float2(
        (vertexID >> 1) != 0 ? 1.0 : -1.0,
        (vertexID & 1) != 0 ? 1.0 : -1.0
    );

    const float2 ndc = clip.xy / clip.w + uv * radiusPx / (viewport * 0.5);

    return float4(ndc * clip.w, clip.z, clip.w);
}
