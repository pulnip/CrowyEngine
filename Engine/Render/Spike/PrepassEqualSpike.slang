// Metal promises the same position from two pipelines only under
// [[invariant]]; the opaque pass's Equal against the prepass measures it.
// PASS: no magenta; the picture equals the golden, drawn with depthPrepass off.
// FAIL: magenta pixels, where Equal missed - drift between the pipelines.

#include "SceneData.slang"

struct VertexOut {
    float4 posProj : SV_Position;
    // flat, because one draw is one group
    nointerpolation float3 color : COLOR0;
};

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    DrawData d = loadDraw(drawID);
    Vertex input = loadVertex(d, vertexID);

    // Playground's position path: to world, then to clip, in two steps
    float4 posWorld = mul(d.world, float4(input.position, 1.0));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.color = loadMaterial(d.materialIndex).albedo;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return float4(input.color, 1.0);
}
