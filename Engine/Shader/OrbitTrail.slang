#include "OrbitCommon.slang"

// Orbit trails as screen-space expanded polylines, plus the marker discs that
// sit on their heads.
//
// One draw per body, one instance per segment, four vertices per instance as a
// triangle strip. bodyIdx arrives as the draw's baseInstance and segIdx as the
// instance index inside that draw, so the same shader serves the direct draws
// here and the single ExecuteIndirect that replaces them later - only where the
// counts come from changes.
//
// Mirrors Engine/RHI/Sample/OrbitFrameSample.cpp; the C++ side asserts the
// offsets spelled out below.

// Mirrors OrbitBodyDraw in OrbitFrameSample.cpp. Four 4-byte scalars rather
// than a float3 and a uint: a float3 struct member is 16-byte aligned in MSL
// and 4-byte aligned in DXIL, and this table is small enough that spelling the
// channels out costs nothing.
struct BodyDraw{
    float colorR;
    float colorG;
    float colorB;
    // segments this body draws; also the denominator of the age fade
    uint segCount;
};

struct TrailPush{
    // the ring, sample-major: [sampleIdx * BODY_COUNT + bodyIdx]
    DescriptorHandle<StructuredBuffer<float3>> samples;   //  0
    DescriptorHandle<StructuredBuffer<BodyDraw>> bodies;  //  8
    float viewportX;                                      // 16
    float viewportY;                                      // 20
    float thicknessPx;                                    // 24
    // heliocentricity: 0 geocentric, 1 heliocentric
    float alpha;                                          // 28
    // next slot to be written; the newest sample is at head - 1
    uint head;                                            // 32
    uint capacity;                                        // 36
    float markerRadiusPx;                                 // 40
    float _pad0;                                          // 44
};

[[vk::push_constant]]
ConstantBuffer<TrailPush> push : register(b0);

// SetGraphicsConstantBuffer slot 0; the push constants own b0
cbuffer frame : register(b1) {
    float4x4 viewProj;
};

struct VertexOut{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct MarkerOut{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float2 viewportSize(){
    return float2(push.viewportX, push.viewportY);
}

// The origin subtracted is the Earth position *at that sample's instant*, not
// the current one. Using the current position would slide every orbit sideways
// and leave the shapes untouched, which is the whole demo gone. Reading both
// out of the same ring slot is what makes that automatic.
float3 framePosition(uint sampleIdx, uint bodyIdx){
    StructuredBuffer<float3> samples = push.samples;

    const uint base = sampleIdx * BODY_COUNT;
    const float3 origin = (1.0 - push.alpha) * samples[base + EARTH_INDEX];

    return samples[base + bodyIdx] - origin;
}

// Newest first, so the segment a body draws never reaches back past its oldest
// stored sample. The ring seam - oldest joined to newest - is not skipped by a
// special case anywhere; this indexing simply never asks for it.
uint slotForAge(uint age){
    return (push.head + push.capacity - 1 - age) % push.capacity;
}

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint segIdx : SV_InstanceID,
    uint bodyIdx : SV_StartInstanceLocation
){
    StructuredBuffer<BodyDraw> bodies = push.bodies;
    const BodyDraw body = bodies[bodyIdx];

    const float3 p0 = orbitLogWarp(framePosition(slotForAge(segIdx), bodyIdx));
    const float3 p1 = orbitLogWarp(framePosition(slotForAge(segIdx + 1), bodyIdx));

    VertexOut output;
    output.position = orbitExpandSegment(
        mul(viewProj, float4(p0, 1.0)),
        mul(viewProj, float4(p1, 1.0)),
        vertexID,
        viewportSize(),
        push.thicknessPx
    );
    output.color = float4(
        body.colorR, body.colorG, body.colorB,
        // age fade: segIdx is the segment's age in ticks
        1.0 - float(segIdx) / float(max(body.segCount, 1u))
    );

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}

// One instance per body rather than one draw: there is no per-draw data to
// index, so the instance index is the body index directly.
[shader("vertex")]
MarkerOut vs_marker(
    uint vertexID : SV_VertexID,
    uint bodyIdx : SV_InstanceID
){
    StructuredBuffer<BodyDraw> bodies = push.bodies;
    const BodyDraw body = bodies[bodyIdx];

    // age 0 is the sample pushed last tick, so the marker lands exactly on the
    // head of the trail rather than near it
    const float3 p = orbitLogWarp(framePosition(slotForAge(0), bodyIdx));

    MarkerOut output;
    output.position = orbitExpandPoint(
        mul(viewProj, float4(p, 1.0)),
        vertexID,
        viewportSize(),
        push.markerRadiusPx,
        output.uv
    );
    // brighter than the trail it caps, but the same hue
    output.color = float4(
        saturate(float3(body.colorR, body.colorG, body.colorB) * 1.35 + 0.12),
        1.0
    );

    return output;
}

[shader("fragment")]
float4 fs_marker(MarkerOut input) : SV_Target {
    const float r = length(input.uv);
    if(r > 1.0)
        discard;

    // one pixel of falloff at the rim, so the disc does not look stepped
    float4 color = input.color;
    color.a *= saturate((1.0 - r) * push.markerRadiusPx);

    return color;
}
