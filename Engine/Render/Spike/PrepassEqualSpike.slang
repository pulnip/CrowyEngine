// Metal promises the same position from two pipelines only under
// [[invariant]]; the opaque pass's Equal against the prepass measures it.
// PASS: no magenta; the picture equals the golden, drawn with depthPrepass off.
// FAIL: magenta pixels, where Equal missed - drift between the pipelines.

#include "ForwardShading.slang"

[shader("fragment")]
float4 fs_main(VertexOut v) : SV_Target {
    return float4(loadMaterial(v.materialIndex).albedo, 1.0);
}
