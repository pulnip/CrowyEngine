// The toon list's prepass: the standard depth, plus the world normal
// makeSurface makes, encoded for the outline to decode.
#include "ForwardShading.slang"

[shader("fragment")]
float4 fs_normals(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = makeSurface(v, frontFace);

    return float4(s.normal * 0.5 + 0.5, 1.0);
}
