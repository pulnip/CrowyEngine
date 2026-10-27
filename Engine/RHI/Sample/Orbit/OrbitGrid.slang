#include "InstanceIndex.slang"
#include "OrbitCommon.slang"

// Not frame-interpolated: alpha subtracts Earth's position, which lies in the
// ecliptic, so the rings' plane never moves.

// Rings a step s apart sit s * pxPerWarpUnit / (1 + R) pixels apart at R AU, so
// the steps form a ladder s * 5^k, each level fading out where its gap shrinks.
static const uint LEVEL_COUNT = 8;
static const uint RINGS_PER_LEVEL = 16;
static const uint RING_SEGMENTS = 192;
static const uint SPOKE_COUNT = 12;

static const uint DRAW_RINGS = 0;
static const uint DRAW_SPOKES = 1;
static const uint DRAW_REFERENCE = 2;

// drawn apart from the ladder, which would style 1 AU by whatever level it
// lands on at the current zoom
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
    float pxPerWarpUnit;    // 40
    float spokeRadiusAU;    // 44
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

// 1-4, 6-9, 11-14, 16-19: the multiples of 5 belong to the level above
uint ringMultiplier(uint slot){
    return slot + 1 + slot / 4;
}

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID,
    uint drawID : SV_StartInstanceLocation
){
    const uint segIdx = zeroBasedInstanceID(instanceID, drawID);
    float3 p0, p1;
    float thickness = push.thinThicknessPx;
    float alpha = push.thinAlpha;

    if(drawID == DRAW_SPOKES){
        // a radial warp keeps a spoke straight, so one segment draws it
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

        const float gapPx = step * push.pxPerWarpUnit / (1.0 + radiusAU);

        alpha *= smoothstep(push.minGapPx, push.fadeGapPx, gapPx);
        if(alpha <= 0.0){
            VertexOut culled;
            culled.position = float4(0.0, 0.0, 0.0, 1.0);
            culled.color = float4(0.0, 0.0, 0.0, 0.0);

            return culled;
        }

        // Thick goes to levels above the finest one still drawn at this radius,
        // not above level 0, or the far field ends up uniformly heavy.
        const float finestLevel = max(0.0, ceil(
            log(push.minGapPx * (1.0 + radiusAU) /
                (push.pxPerWarpUnit * push.baseStepAU)) / log(5.0)
        ));
        if(float(level) > finestLevel + 0.5){
            thickness = push.thickThicknessPx;
            alpha = push.thickAlpha;
        }

        p0 = circlePoint(radiusAU, arcIdx, RING_SEGMENTS);
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
