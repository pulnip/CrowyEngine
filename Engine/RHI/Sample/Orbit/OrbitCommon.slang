#pragma once

// Shared rather than copied: the grid only lines up with the trails if both go
// through exactly the same warp.

// keep in sync with ORBIT_BODY_COUNT / ORBIT_EARTH_INDEX in Kepler.hpp
static const uint BODY_COUNT = 9;
static const uint EARTH_INDEX = 3;

// Mirrors OrbitBodyDraw in OrbitDrawArgs.hpp. Scalars only: a float3 member is
// 16-byte aligned in MSL and 4-byte aligned in DXIL.
struct BodyDraw{
    float colorR;
    float colorG;
    float colorB;
    float trailPeriodDays;
};

static const float ORBIT_PI = 3.14159265358979324;

float orbitWrapToPi(float radians){
    return radians - 6.28318530717958648 *
        floor((radians + ORBIT_PI) * 0.159154943091895336);
}

// log(1 + r) in AU turns Neptune:Mercury from 77:1 into 10:1. It runs after the
// frame subtraction; subtracting in the nonlinear space breaks the shapes.
float3 orbitLogWarp(float3 pLin){
    const float r = length(pLin);

    return r > 1e-6 ? pLin * (log(1.0 + r) / r) : float3(0.0, 0.0, 0.0);
}

// The perpendicular is taken in pixels: NDC is not square on a non-square
// viewport, so a perpendicular there shears every line not axis-aligned.
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
    // the Sun's trail is all zero-length segments at alpha = 1
    const float2 dirPx = lengthPx > 1e-6 ?
        deltaPx / lengthPx :
        float2(1.0, 0.0);
    const float2 perpPx = float2(-dirPx.y, dirPx.x) * (thicknessPx * 0.5) * side;

    const float4 clip = endpoint == 0 ? c0 : c1;
    const float2 ndc = (endpoint == 0 ? ndc0 : ndc1) + perpPx / halfViewport;

    // undoes the rasterizer's divide; w is 1 under an orthographic camera
    return float4(ndc * clip.w, clip.z, clip.w);
}

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
