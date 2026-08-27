#include "InstanceIndex.slang"
#include "OrbitCommon.slang"

// Orbit trails as screen-space expanded polylines, plus the marker discs that
// sit on their heads.
//
// One draw per body, one instance per segment, four vertices per instance as a
// triangle strip. bodyIdx arrives as the draw's baseInstance and segIdx as the
// instance index inside that draw.
//
// The nine draws are issued as one ExecuteIndirect over arguments a compute
// pass writes, and this shader never learned about it: reading the body index
// off the draw rather than off a uniform is exactly what made that swap free.
//
// Mirrors Engine/RHI/Sample/OrbitFrame.cpp; the C++ side asserts the
// offsets spelled out below.

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
    // how much of Earth's orbital angle to cancel: 0 leaves the frame
    // inertial, 1 makes it co-rotate with Earth
    float frameLock;                                      // 44
    // -frameLock * (Earth's unwrapped ecliptic longitude at the newest
    // sample), already reduced into (-pi, pi] on the CPU in double. See
    // frameRotation below for why the unwrapping cannot happen here.
    float frameLockBase;                                  // 48
    // Earth's mean motion, radians per day
    float earthRadPerDay;                                 // 52
    float dayPerSample;                                   // 56
    float beltRadiusPx;                                   // 60
    // heliocentric asteroid positions at the current instant, one per dot
    DescriptorHandle<StructuredBuffer<float3>> belt;      // 64
    float beltAlpha;                                      // 72
    // dot 0 of this frame's slice inside the shared transient buffer
    uint beltBase;                                        // 76
    // segments per body, written every frame by cs_trail_args alongside the
    // indirect arguments, so the fade denominator and the instance count come
    // out of one pass and cannot drift
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

// Newest first, so the segment a body draws never reaches back past its oldest
// stored sample. The ring seam - oldest joined to newest - is not skipped by a
// special case anywhere; this indexing simply never asks for it.
uint slotForAge(uint age){
    return (push.head + push.capacity - 1 - age) % push.capacity;
}

// The angle the co-rotating frame is turned by for a sample `age` ticks old.
//
// It has to be -frameLock * theta(t) with theta CONTINUOUS in t. atan2 alone is
// not: it jumps by 2*pi every time Earth crosses the -x axis, and multiplying
// that by a fractional lock turns the jump into a real 2*pi*frameLock break -
// a straight line torn across the trail once per year. Only frameLock 0 and 1
// survive it, because a rotation by 2*pi is the identity.
//
// So the angle is rebuilt from two pieces that are each well behaved:
//   - the base, -frameLock * theta(newest), unwrapped and reduced on the CPU
//     where the sim day is still a double. Reducing it mod 2*pi is free: it is
//     a whole rotation.
//   - the angle Earth actually swept between this sample and the newest one,
//     which is the mean motion times the elapsed days plus a correction under
//     4 degrees. Taking that correction through wrapToPi recovers the exact
//     swept angle without ever needing an absolute time in a float.
float frameRotation(float3 earthThen, float3 earthNow, uint age){
    const float meanSwept = push.earthRadPerDay *
        float(age) * push.dayPerSample;
    const float swept = meanSwept + orbitWrapToPi(
        (atan2(earthNow.y, earthNow.x) - atan2(earthThen.y, earthThen.x)) -
        meanSwept
    );

    return push.frameLockBase + push.frameLock * swept;
}

// Puts a heliocentric position into the frame the panel is asking for:
// the origin slides from Earth to the Sun with alpha, and the whole thing can
// be spun with Earth on top of that.
//
// Both happen before the warp. For the subtraction that is load-bearing - the
// warp is not linear, so subtracting after it would not be the same frame. For
// the rotation it makes no difference (an isotropic radial scale commutes with
// a rotation); it lives here because it belongs next to the subtraction.
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

// The Earth position handed to toFrame is the one from *that sample's instant*,
// not the current one. Using the current position would slide every orbit
// sideways and leave the shapes untouched, which is the whole demo gone.
// Reading both out of the same ring slot is what makes that automatic.
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
        // age fade: segIdx is the segment's age in ticks
        1.0 - float(segIdx) / float(max(segCounts[bodyIdx], 1u))
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
    StructuredBuffer<uint> segCounts = push.segCounts;

    // a body switched off in the panel draws no segments, and its marker goes
    // with them
    if(segCounts[bodyIdx] == 0){
        MarkerOut culled;
        culled.position = float4(0.0, 0.0, 0.0, 1.0);
        culled.color = float4(0.0, 0.0, 0.0, 0.0);
        culled.uv = float2(0.0, 0.0);

        return culled;
    }

    // age 0 is the sample pushed last tick, so the marker lands exactly on the
    // head of the trail rather than near it
    const float3 p = orbitLogWarp(framePosition(0, bodyIdx));

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

// The asteroid belt: current positions only, no trails - a few thousand
// overlapping ones would be a grey smear. Positions come in already
// heliocentric, so they take the same frame treatment as everything else and
// break into epicycles at alpha 0 with the planets.
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

    // one pixel of falloff at the rim, so the disc does not look stepped
    float4 color = input.color;
    color.a *= saturate((1.0 - r) * push.markerRadiusPx);

    return color;
}
