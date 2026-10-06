// M_PaintBrush: one splat stamped over its atlas rectangles. Opaque and
// unblended, it reads the previous paint itself and writes the whole texel,
// so an id is replaced, never blended; the result lands in a scratch target
// and PaintCopy.slang takes the rectangles back.

#include "PaintShared.h"
#include "SplatStamp.slang"

// mirrored from PaintBrushPush in PaintLabGpu.hpp
struct BrushPush {
    DescriptorHandle<Texture2D> previous;
    DescriptorHandle<Texture2D> position;
    // the scaled-local bounds the position atlas un-normalizes with
    float4 boundsMin;
    float4 boundsSize;
    // xyz centre, w radius
    float4 centerRadius;
    // xyz U axis, w stretch
    float4 axisUStretch;
    // xyz V axis, w ImpactU
    float4 axisVImpact;
    // x seed, y height add, z id / 255, w star generation
    float4 splat;
    float4 lockGens;
    // x BrushShapeNoise, y shape stage, z distance range
    float4 shape;
};

[[vk::push_constant]]
ConstantBuffer<BrushPush> brush : register(b0);

struct BrushOut {
    float4 position : SV_Position;
};

// one triangle over the atlas; the scissor keeps it to one rectangle
[shader("vertex")]
BrushOut vs_brush(uint vertexID : SV_VertexID) {
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    BrushOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_brush(BrushOut input) : SV_Target {
    // the pixel over an atlas texel reads exactly that texel
    int3 texel = int3(int2(input.position.xy), 0);
    Texture2D previous = brush.previous;
    Texture2D positions = brush.position;
    float4 prev = previous.Load(texel);
    float3 normalized = positions.Load(texel).xyz;

    // MF_ProjectOntoBasis: the texel's scaled-local point in the stamp basis
    float3 local = normalized * brush.boundsSize.xyz + brush.boundsMin.xyz;
    float3 axisU = brush.axisUStretch.xyz;
    float3 axisV = brush.axisVImpact.xyz;
    float3 offset = local - brush.centerRadius.xyz;
    float radius = brush.centerRadius.w;
    float stretch = brush.axisUStretch.w;
    float2 uv = float2(
        dot(offset, axisU) / (radius * stretch),
        dot(offset, axisV) / radius
    );
    float n = dot(offset, cross(axisU, axisV)) / radius;

    float Seed = brush.splat.x;
    float ImpactU = brush.axisVImpact.w;
    float3 stamp = brush.shape.y >= PAINT_SHAPE_STAGE_FULL
        ? StampCustom(uv, n, Seed, ImpactU, brush.shape.x)
        : StampStaged(uv, stretch, n, Seed, ImpactU, brush.shape.y);
    float Mask = stamp.x;
    float Kernel = stamp.y;
    float Dist = stamp.z;

    float PrevR = prev.r;
    float PrevG = prev.g;
    float PrevB = prev.b;
    float IdEnc = brush.splat.z;
    float HeightAdd = brush.splat.y;
    float DistRange = brush.shape.z;
    float StarGen = brush.splat.w;
    float4 LockGens = brush.lockGens;

    // M_PaintBrush's Custom node from here, verbatim but for syntax.
    // B holds 1 - d / DistRange, d being the distance in texels from a texel
    // to the nearest edge of its own region, so 0 reads as far. The stamp
    // distance comes in stamp units; one screen derivative in this pass is
    // one texel, so dividing by the gradient converts it to texels. R holds
    // the paint id in its low three bits and a star generation above.
    float prevByte = round(PrevR * 255.0);
    float prevGen = floor(prevByte / 8.0);
    float prevId = prevByte - 8.0 * prevGen;
    float newId = round(IdEnc * 255.0);
    bool eraser = newId > 6.5;
    float newGen = eraser ? 0.0 : round(StarGen);
    bool sameId = abs(prevId - newId) < 0.5;
    float lockGen = dot(LockGens, 1.0 - step(0.5, abs(float4(0.0, 1.0, 2.0, 3.0) - prevId)));
    bool prevLocked = prevGen > 0.5 && abs(prevGen - lockGen) < 0.5;
    float dOld = (1.0 - PrevB) * DistRange;
    float grad = max(length(float2(ddx(Dist), ddy(Dist))), 1e-4);
    float dBrush = clamp(Dist / grad, -DistRange, DistRange);
    float dNew;
    if(Mask > 0.5) {
        dNew = sameId ? max(dOld, -dBrush) : -dBrush;
    } else {
        dNew = sameId ? dOld : min(dOld, dBrush);
    }
    float distEnc = saturate(1.0 - dNew / DistRange);
    float accum = saturate(PrevG + Kernel * HeightAdd);
    float erased = PrevG * (1.0 - Mask);
    // painting the none id is the eraser: it takes the height away too
    float h = eraser ? erased : accum;
    float outGen = sameId ? (prevLocked ? prevGen : newGen) : newGen;
    float outByte = (Mask > 0.0) ? newId + 8.0 * outGen : prevByte;
    float3 painted = float3(outByte / 255.0, h, distEnc);
    float3 result = (prevLocked && !sameId) ? float3(PrevR, PrevG, PrevB) : painted;

    return float4(result, 1.0);
}
