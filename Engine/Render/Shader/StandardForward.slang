// The standard passes' fragment entries: a material's surface, shaded by
// the model it links, and a Masked one's cut.

#include "ForwardShading.slang"

[shader("fragment")]
float4 fs_opaque(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = loadSurface(v, frontFace);

    return float4(sceneColor(s, v), 1.0);
}

// blended over the opaque scene in linear radiance, before the tone map
[shader("fragment")]
float4 fs_translucent(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = loadSurface(v, frontFace);

    return float4(sceneColor(s, v), s.opacity);
}

// opaque where the surface reaches its cutoff
[shader("fragment")]
float4 fs_masked(VertexOut v, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = loadSurface(v, frontFace);
    cutMasked(s, v.materialIndex);

    return float4(sceneColor(s, v), 1.0);
}

// the same cut in the depth-only passes, which write no color
[shader("fragment")]
void fs_masked_depth(VertexOut v, bool frontFace : SV_IsFrontFace) {
    cutMasked(loadSurface(v, frontFace), v.materialIndex);
}
