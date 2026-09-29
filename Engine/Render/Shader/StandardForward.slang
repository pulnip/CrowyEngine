// The standard colour passes' fragment entries: a material's surface,
// shaded by the model it links.

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
