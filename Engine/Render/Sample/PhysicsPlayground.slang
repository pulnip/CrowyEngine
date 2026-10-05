// The physics playground's floor: the engine's forward loop with the
// sample grid darkening its albedo.
#include "ForwardShading.slang"
#include "Grid.slang"

[shader("fragment")]
float4 fs_grid(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = loadSurface(v, frontFace);
    s.albedo *= 1.0 - gridDarkening(v.posWorld, s.normal);

    return float4(sceneColor(s, v), 1.0);
}
