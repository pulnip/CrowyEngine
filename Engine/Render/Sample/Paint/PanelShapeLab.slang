// The stamp alone, in a corner of the back buffer: one splat's signed
// distance laid flat in world proportion (U / R across, V / R up), at any
// stop of the shape slider. Display values, written as is.

#include "PaintCommon.slang"
#include "SplatStamp.slang"

// mirrored from PaintShapeLabPush in PaintLabGpu.hpp
struct ShapeLabPush {
    // the panel in back-buffer pixels: x, y, width, height
    float4 rect;
    // x seed, y ImpactU, z stretch, w shape stage
    float4 stamp;
    // x BrushShapeNoise, y half the panel's extent in U / R, z the team
    float4 look;
};

[[vk::push_constant]]
ConstantBuffer<ShapeLabPush> lab : register(b0);

struct ShapeLabOut {
    float4 position : SV_Position;
};

[shader("vertex")]
ShapeLabOut vs_shapelab(uint vertexID : SV_VertexID) {
    static const float2 corners[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    ShapeLabOut output;
    output.position = float4(corners[vertexID], 0.0, 1.0);

    return output;
}

[shader("fragment")]
float4 fs_shapelab(ShapeLabOut input) : SV_Target {
    float2 t = (input.position.xy - lab.rect.xy) / lab.rect.zw;
    float extent = lab.look.y;
    // U / R to the right, V / R up
    float2 world = float2(t.x * 2.0 - 1.0, 1.0 - t.y * 2.0) * extent;
    float stretch = lab.stamp.z;
    float2 uv = float2(world.x / stretch, world.y);

    float3 stamp = lab.stamp.w >= PAINT_SHAPE_STAGE_FULL
        ? StampCustom(uv, 0.0, lab.stamp.x, lab.stamp.y, lab.look.x)
        : StampStaged(uv, stretch, 0.0, lab.stamp.x, lab.stamp.y, lab.stamp.w);
    float d = stamp.z;
    float pixel = 2.0 * extent / lab.rect.z;

    float3 background = float3(0.11, 0.11, 0.12);
    float3 team = TeamDisplayColor(uint(lab.look.z));
    float3 color = background;
    if(stamp.x > 0.5) {
        // the deposit kernel: full in the core, feathered over the rim
        color = lerp(team * 0.55, team, stamp.y);
    }

    // iso-distance lines every 0.1 outside, and the edge itself
    float iso = abs(frac(d / 0.1 + 0.5) - 0.5) * 0.1;
    if(d > 0.0 && iso < pixel * 0.6)
        color = lerp(color, float3(0.45, 0.45, 0.48), 0.6);
    if(abs(d) < pixel * 1.2)
        color = float3(0.97, 0.97, 0.95);

    // the guides: the main disc's radius 0.5 and the stamp's reach |uv| = 1
    float disc = abs(length(uv) - 0.5) * min(stretch, 1.0);
    float reach = abs(length(uv) - 1.0);
    float dash = frac(atan2(world.y, world.x) * 12.0 / 6.2831853) < 0.5 ? 1.0 : 0.0;
    if(disc < pixel * 0.8 && dash > 0.5)
        color = lerp(color, float3(0.62, 0.89, 0.83), 0.7);
    if(reach < pixel * 0.8 && dash > 0.5)
        color = lerp(color, float3(0.55, 0.55, 0.58), 0.7);

    return float4(color, 1.0);
}
