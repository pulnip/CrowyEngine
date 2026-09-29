// The prototyping map's own surface: the engine's forward loop, with a
// world-aligned grid darkening the albedo of the floor, walls and blocks.
#include "ForwardShading.slang"

static const float GridMajor = 1.0;
static const float GridMinor = 0.25;

// 1 where a line of this spacing crosses the pixel, fading out before the
// cells shrink below a few pixels and turn to noise
float gridLine(float2 coord, float2 footprint, float spacing) {
    float2 toLine = abs(frac(coord / spacing - 0.5) - 0.5) * spacing;
    float2 pixels = toLine / max(footprint, 1e-5);
    float cellPixels = spacing / max(max(footprint.x, footprint.y), 1e-5);

    return (1.0 - saturate(min(pixels.x, pixels.y))) *
        saturate((cellPixels - 2.0) / 4.0);
}

// How much a world-aligned grid darkens the albedo here, drawn in the plane
// facing the normal's dominant axis, so floors and walls both carry it
float gridDarkening(float3 posWorld, float3 N) {
    // derivatives first, in uniform control flow: inside a branch the
    // neighbouring pixels might not take, they are undefined. Scalar fwidth,
    // because the vector overload does not target Metal
    float3 footprint = float3(
        fwidth(posWorld.x),
        fwidth(posWorld.y),
        fwidth(posWorld.z)
    );

    float3 a = abs(N);
    float2 coord = posWorld.xy;
    float2 width = footprint.xy;
    if(a.y >= a.x && a.y >= a.z) {
        coord = posWorld.xz;
        width = footprint.xz;
    } else if(a.x >= a.z) {
        coord = posWorld.zy;
        width = footprint.zy;
    }

    float major = gridLine(coord, width, GridMajor);
    float minor = gridLine(coord, width, GridMinor);

    return max(0.55 * major, 0.25 * minor);
}

[shader("fragment")]
float4 fs_grid(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = loadSurface(v, frontFace);
    s.albedo *= 1.0 - gridDarkening(v.posWorld, s.normal);

    return float4(sceneColor(s, v), 1.0);
}
