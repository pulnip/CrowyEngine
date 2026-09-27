// Does the opaque pass, testing Equal, find exactly the depth the prepass
// wrote with the same vertex shader in another pipeline?
//
// RenderApp draws a depth-only prepass (Less, write) and then the opaque
// round at Equal without writing. The two pipelines run this file's vs_main
// separately; if any position differs by a bit between them, Equal fails
// there and the magenta clear shows through. Metal promises that invariance
// only under [[invariant]]; on either backend it is measured, not assumed.
//
// The scene stresses it: interpenetrating pairs, dense spheres, rotated and
// non-uniformly scaled boxes, a floor seen at grazing angles, and objects
// near the far plane, where D32 depths are coarsest. A floor and a far wall
// cover every pixel no object covers, and each group has its own flat
// color from its material row.
//
//   PASS: no magenta anywhere; the picture equals the one drawn with
//         debug.depthPrepass off, which is the golden.
//   FAIL: magenta pixels are Equal misses - drift between the pipelines.
//         Another group's color on a contact line would be a tie, which
//         this scene rules out: a pair shares one group and nothing else
//         touches.

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
    DrawData d = LoadDraw(drawID);
    Vertex input = LoadVertex(d, vertexID);

    // Playground's position path: to world, then to clip, in two steps
    float4 posWorld = mul(d.world, float4(input.position, 1.0));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.color = LoadMaterial(d.materialIndex).albedo;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return float4(input.color, 1.0);
}
