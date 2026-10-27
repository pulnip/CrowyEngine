#include "InstanceIndex.slang"
#include "OrbitCommon.slang"

// Mirrors TrailPush in OrbitFrame.cpp.
struct TrailPush{
    DescriptorHandle<StructuredBuffer<float3>> samples;   //  0
    DescriptorHandle<StructuredBuffer<BodyDraw>> bodies;  //  8
    float viewportX;                                      // 16
    float viewportY;                                      // 20
    float thicknessPx;                                    // 24
    // 0 geocentric, 1 heliocentric
    float alpha;                                          // 28
    uint head;                                            // 32
    uint capacity;                                        // 36
    float markerRadiusPx;                                 // 40
    // 0 inertial, 1 co-rotating with Earth
    float frameLock;                                      // 44
    // -frameLock * Earth's unwrapped longitude at the newest sample, reduced on
    // the CPU in double
    float frameLockBase;                                  // 48
    float earthRadPerDay;                                 // 52
    float dayPerSample;                                   // 56
    float beltRadiusPx;                                   // 60
    DescriptorHandle<StructuredBuffer<float3>> belt;      // 64
    float beltAlpha;                                      // 72
    uint beltBase;                                        // 76
    // written by cs_trail_args with the draw arguments, so the fade and the
    // instance count cannot drift
    DescriptorHandle<StructuredBuffer<uint>> segCounts;   // 80
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

uint slotForAge(uint age){
    return (push.head + push.capacity - 1 - age) % push.capacity;
}

// atan2's yearly 2*pi jump times a fractional frameLock tears the trail, so the
// angle is the CPU's unwrapped base plus the angle swept since that sample.
float frameRotation(float3 earthThen, float3 earthNow, uint age){
    const float meanSwept = push.earthRadPerDay *
        float(age) * push.dayPerSample;
    const float swept = meanSwept + orbitWrapToPi(
        (atan2(earthNow.y, earthNow.x) - atan2(earthThen.y, earthThen.x)) -
        meanSwept
    );

    return push.frameLockBase + push.frameLock * swept;
}

float3 toFrame(float3 p, float3 earthThen, uint age){
    float3 out = p - (1.0 - push.alpha) * earthThen;

    if(push.frameLock > 0.0){
        StructuredBuffer<float3> samples = push.samples;
        const float3 earthNow =
            samples[slotForAge(0) * BODY_COUNT + EARTH_INDEX];

        const float angle = frameRotation(earthThen, earthNow, age);
        const float c = cos(angle), s = sin(angle);

        out.xy = float2(c * out.x - s * out.y, s * out.x + c * out.y);
    }

    return out;
}

// Earth from that sample's own instant: the current one would only slide each
// orbit sideways and leave its shape untouched.
float3 framePosition(uint age, uint bodyIdx){
    StructuredBuffer<float3> samples = push.samples;

    const uint base = slotForAge(age) * BODY_COUNT;

    return toFrame(samples[base + bodyIdx], samples[base + EARTH_INDEX], age);
}

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID,
    uint bodyIdx : SV_StartInstanceLocation
){
    const uint segIdx = zeroBasedInstanceID(instanceID, bodyIdx);
    StructuredBuffer<BodyDraw> bodies = push.bodies;
    const BodyDraw body = bodies[bodyIdx];
    StructuredBuffer<uint> segCounts = push.segCounts;

    const float3 p0 = orbitLogWarp(framePosition(segIdx, bodyIdx));
    const float3 p1 = orbitLogWarp(framePosition(segIdx + 1, bodyIdx));

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
        1.0 - float(segIdx) / float(max(segCounts[bodyIdx], 1u))
    );

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}

// one instance per body: with no per-draw data, the instance is the body
[shader("vertex")]
MarkerOut vs_marker(
    uint vertexID : SV_VertexID,
    uint bodyIdx : SV_InstanceID
){
    StructuredBuffer<BodyDraw> bodies = push.bodies;
    const BodyDraw body = bodies[bodyIdx];
    StructuredBuffer<uint> segCounts = push.segCounts;

    // a body switched off in the panel has no segments
    if(segCounts[bodyIdx] == 0){
        MarkerOut culled;
        culled.position = float4(0.0, 0.0, 0.0, 1.0);
        culled.color = float4(0.0, 0.0, 0.0, 0.0);
        culled.uv = float2(0.0, 0.0);

        return culled;
    }

    const float3 p = orbitLogWarp(framePosition(0, bodyIdx));

    MarkerOut output;
    output.position = orbitExpandPoint(
        mul(viewProj, float4(p, 1.0)),
        vertexID,
        viewportSize(),
        push.markerRadiusPx,
        output.uv
    );
    output.color = float4(
        saturate(float3(body.colorR, body.colorG, body.colorB) * 1.35 + 0.12),
        1.0
    );

    return output;
}

// no belt trails: a few thousand overlapping ones would be a grey smear
[shader("vertex")]
MarkerOut vs_belt(
    uint vertexID : SV_VertexID,
    uint asteroidIdx : SV_InstanceID
){
    StructuredBuffer<float3> samples = push.samples;
    StructuredBuffer<float3> belt = push.belt;

    const float3 earthNow =
        samples[slotForAge(0) * BODY_COUNT + EARTH_INDEX];
    const float3 p = orbitLogWarp(
        toFrame(belt[push.beltBase + asteroidIdx], earthNow, 0)
    );

    MarkerOut output;
    output.position = orbitExpandPoint(
        mul(viewProj, float4(p, 1.0)),
        vertexID,
        viewportSize(),
        push.beltRadiusPx,
        output.uv
    );
    output.color = float4(0.62, 0.60, 0.58, push.beltAlpha);

    return output;
}

[shader("fragment")]
float4 fs_marker(MarkerOut input) : SV_Target {
    const float r = length(input.uv);
    if(r > 1.0)
        discard;

    float4 color = input.color;
    color.a *= saturate((1.0 - r) * push.markerRadiusPx);

    return color;
}
