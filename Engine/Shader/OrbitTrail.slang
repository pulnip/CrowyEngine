// Orbit trails as screen-space expanded polylines.
//
// One draw per body, one instance per segment, four vertices per instance as a
// triangle strip. bodyIdx arrives as the draw's baseInstance and segIdx as the
// instance index inside that draw, so the same shader serves the direct draws
// here and the single ExecuteIndirect that replaces them later - only where the
// counts come from changes.
//
// Mirrors Engine/RHI/Sample/OrbitFrameSample.cpp; the C++ side asserts the
// offsets spelled out below.

// keep in sync with ORBIT_BODY_COUNT / ORBIT_EARTH_INDEX in Kepler.hpp
static const uint BODY_COUNT = 9;
static const uint EARTH_INDEX = 3;

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

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint segIdx : SV_InstanceID,
    uint bodyIdx : SV_StartInstanceLocation
){
    StructuredBuffer<BodyDraw> bodies = push.bodies;
    const BodyDraw body = bodies[bodyIdx];

    // Newest first, so the segment a body draws never reaches back past its
    // oldest stored sample. The ring seam - oldest joined to newest - is not
    // skipped by a special case here; this indexing simply never asks for it.
    const uint i0 = (push.head + push.capacity - 1 - segIdx) % push.capacity;
    const uint i1 = (push.head + push.capacity - 2 - segIdx) % push.capacity;

    const float3 p0 = framePosition(i0, bodyIdx);
    const float3 p1 = framePosition(i1, bodyIdx);

    const float4 c0 = mul(viewProj, float4(p0, 1.0));
    const float4 c1 = mul(viewProj, float4(p1, 1.0));

    const float2 ndc0 = c0.xy / c0.w;
    const float2 ndc1 = c1.xy / c1.w;

    // strip order: (p0,-1) (p0,+1) (p1,-1) (p1,+1)
    const uint endpoint = vertexID >> 1;
    const float side = (vertexID & 1) != 0 ? 1.0 : -1.0;

    const float2 viewport = float2(push.viewportX, push.viewportY);
    const float2 halfViewport = viewport * 0.5;

    // the perpendicular is taken in pixels, not in NDC. NDC units are not
    // square on a non-square viewport, so a perpendicular computed there
    // shears every line that is not axis-aligned
    const float2 deltaPx = (ndc1 - ndc0) * halfViewport;
    const float lengthPx = length(deltaPx);
    // a zero-length segment has no direction to be perpendicular to; the Sun's
    // trail is entirely made of them at alpha = 1
    const float2 dirPx = lengthPx > 1e-6 ?
        deltaPx / lengthPx :
        float2(1.0, 0.0);
    const float2 perpPx = float2(-dirPx.y, dirPx.x) *
        (push.thicknessPx * 0.5) * side;

    const float4 clip = endpoint == 0 ? c0 : c1;
    const float2 ndc = (endpoint == 0 ? ndc0 : ndc1) + perpPx / halfViewport;

    // back through the perspective divide the rasterizer is about to redo.
    // The camera is orthographic here, so w is 1 and this is a no-op - it is
    // written out anyway so the off-ecliptic camera later costs nothing.
    VertexOut output;
    output.position = float4(ndc * clip.w, clip.z, clip.w);
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
