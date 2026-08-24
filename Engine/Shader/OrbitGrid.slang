#include "OrbitCommon.slang"

// The polar grid the trails are read against: concentric rings at a regular AU
// step and spokes every 30 degrees.
//
// Every point goes through orbitLogWarp, the same function the trails use. A
// ring drawn at a constant screen radius would be a lie the moment the warp
// changed; routing it through the shared function means it cannot drift.
//
// The grid is not frame-interpolated. What alpha subtracts is Earth's position,
// which lies exactly in the ecliptic - so the plane the rings live in never
// moves, and they stay the fixed reference the trails deform against.
//
// ## Why the grid needs levels of detail
//
// Under the warp a ring at R AU lands at log(1 + R), so the screen gap between
// two rings a step `s` apart is
//
//     gap(R) = s * pxPerWarpUnit / (1 + R)
//
// which shrinks without bound as R grows. One fixed step is therefore legible
// only over a band of radii: too coarse near the origin, a solid smear far out.
//
// So the grid is drawn at a ladder of steps s * 5^k. Level k covers the band
// where its own gap reads well and fades out where it does not, and because
// each level skips the multiples of 5 that the next level up owns, every
// radius is drawn exactly once. Eight levels span a factor of 5^7 * 20, which
// puts the outermost ring past the screen corner at every zoom this sample
// allows - the grid has no edge to run into.

static const uint LEVEL_COUNT = 8;
static const uint RINGS_PER_LEVEL = 16;
static const uint RING_SEGMENTS = 192;
static const uint SPOKE_COUNT = 12;

static const uint DRAW_RINGS = 0;
static const uint DRAW_SPOKES = 1;
static const uint DRAW_REFERENCE = 2;

// the ring the whole demo turns on: at alpha 0 the Sun's trail sits on it, at
// alpha 1 Earth's does, and pushing the slider swaps the two over it. It is
// drawn on its own rather than left to the ladder, which would style it by
// whatever level 1 AU happened to land on at the current zoom.
static const float REFERENCE_RING_AU = 1.0;

struct GridPush{
    float viewportX;        //  0
    float viewportY;        //  4
    float thinThicknessPx;  //  8
    float thickThicknessPx; // 12
    float colorR;           // 16
    float colorG;           // 20
    float colorB;           // 24
    float thinAlpha;        // 28
    float thickAlpha;       // 32
    // step of the finest level, always a power of 5 AU
    float baseStepAU;       // 36
    // screen pixels per unit of warped radius
    float pxPerWarpUnit;    // 40
    // spokes run to here: the warped screen corner, turned back into AU
    float spokeRadiusAU;    // 44
    // a ring is dropped below this screen gap and fully lit above the next
    float minGapPx;         // 48
    float fadeGapPx;        // 52
    float referenceAlpha;   // 56
    float _pad0;            // 60
};

[[vk::push_constant]]
ConstantBuffer<GridPush> push : register(b0);

cbuffer frame : register(b1) {
    float4x4 viewProj;
};

struct VertexOut{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

float3 circlePoint(float radiusAU, uint step, uint stepCount){
    const float theta = 6.2831853071795865 * float(step) / float(stepCount);

    return float3(radiusAU * cos(theta), radiusAU * sin(theta), 0.0);
}

// 1,2,3,4, 6,7,8,9, 11,12,13,14, 16,17,18,19 - every multiple of the level's
// own step up to 20 except the multiples of 5, which belong to the level above
uint ringMultiplier(uint slot){
    return slot + 1 + slot / 4;
}

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint segIdx : SV_InstanceID,
    uint drawID : SV_StartInstanceLocation
){
    float3 p0, p1;
    float thickness = push.thinThicknessPx;
    float alpha = push.thinAlpha;

    if(drawID == DRAW_SPOKES){
        // a radial line stays radial through a radial warp, so one segment from
        // the origin out to the corner is the whole spoke
        p0 = float3(0.0, 0.0, 0.0);
        p1 = circlePoint(push.spokeRadiusAU, segIdx, SPOKE_COUNT);
    }
    else if(drawID == DRAW_REFERENCE){
        p0 = circlePoint(REFERENCE_RING_AU, segIdx, RING_SEGMENTS);
        p1 = circlePoint(REFERENCE_RING_AU, (segIdx + 1) % RING_SEGMENTS, RING_SEGMENTS);
        thickness = push.thickThicknessPx;
        alpha = push.referenceAlpha;
    }
    else{
        const uint arcIdx = segIdx % RING_SEGMENTS;
        const uint ringSlot = segIdx / RING_SEGMENTS;
        const uint level = ringSlot / RINGS_PER_LEVEL;
        const uint slot = ringSlot % RINGS_PER_LEVEL;

        const float step = push.baseStepAU * pow(5.0, float(level));
        const float radiusAU = float(ringMultiplier(slot)) * step;

        // how far this ring sits from its neighbours on screen
        const float gapPx = step * push.pxPerWarpUnit / (1.0 + radiusAU);

        alpha *= smoothstep(push.minGapPx, push.fadeGapPx, gapPx);
        if(alpha <= 0.0){
            // too dense to read here; collapse the quad rather than smear it
            VertexOut culled;
            culled.position = float4(0.0, 0.0, 0.0, 1.0);
            culled.color = float4(0.0, 0.0, 0.0, 0.0);

            return culled;
        }

        // The heavier weight goes to whichever rings are five steps apart
        // *here*, which is not a fixed level: past the radius where the base
        // step has faded out, the level above it becomes the fine grid and has
        // to be drawn thin, or the far field ends up uniformly heavy.
        //
        // finestLevel is the lowest level still drawn at this radius - the one
        // whose gap has not yet fallen under minGapPx - floored at the base,
        // since nothing finer exists.
        const float finestLevel = max(0.0, ceil(
            log(push.minGapPx * (1.0 + radiusAU) /
                (push.pxPerWarpUnit * push.baseStepAU)) / log(5.0)
        ));
        if(float(level) > finestLevel + 0.5){
            thickness = push.thickThicknessPx;
            alpha = push.thickAlpha;
        }

        p0 = circlePoint(radiusAU, arcIdx, RING_SEGMENTS);
        // the last arc closes onto step 0, so the ring has no gap
        p1 = circlePoint(radiusAU, (arcIdx + 1) % RING_SEGMENTS, RING_SEGMENTS);
    }

    VertexOut output;
    output.position = orbitExpandSegment(
        mul(viewProj, float4(orbitLogWarp(p0), 1.0)),
        mul(viewProj, float4(orbitLogWarp(p1), 1.0)),
        vertexID,
        float2(push.viewportX, push.viewportY),
        thickness
    );
    output.color = float4(push.colorR, push.colorG, push.colorB, alpha);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
