// The prototyping map's own surface: the engine's forward loop, with a
// world-aligned grid darkening the albedo of the floor, walls and blocks.
#include "ForwardShading.slang"
#include "Grid.slang"

[shader("fragment")]
float4 fs_grid(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = makeSurface(v, frontFace);
    s.albedo *= 1.0 - gridDarkening(v.posWorld, s.normal);

    return float4(sceneColor(s, v), 1.0);
}
