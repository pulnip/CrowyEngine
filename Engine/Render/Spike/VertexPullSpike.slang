// Does a mesh that does not start at row 0 of the shared vertex pool pull its
// own vertices?
//
// SV_VertexID on an indexed draw is the raw index buffer value on D3D12, while
// Metal's [[vertex_id]] folds the draw's baseVertex into it - the same split
// zeroBasedInstanceID papers over for SV_InstanceID. Rather than branch per
// target, a DrawList draws with baseVertex 0 and carries the pool offset in
// DrawData::vbIndex, so every backend reads d.vbIndex + vertexID.
//
// Four polygons of 3, 4, 5 and 6 sides, each its own pool allocation, so only
// the first one sits at offset 0. The colour comes from objectID rather than
// from the vertices, which keeps a bad pull readable as a bad shape.
//
//   PASS: 3, 4, 5, 6 sides left to right - red, green, blue, yellow.
//   FAIL: any other shape. Every draw but the first read the pool from row 0,
//         which is what putting the offset back into baseVertex does on D3D12.
//
// Every Render shader pulls through LoadVertex (Engine/Shader/SceneData.slang),
// and so does this spike. Before the contract was fixed, each sample lost every
// mesh but the first on D3D12.

#include "SceneData.slang"

// keep in sync with VertexPullSpike.cpp
static const uint SLOT_COUNT = 4;

struct VertexOut{
    float4 posProj : SV_Position;
    float4 color : COLOR0;
};

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    static const float3 SLOT_COLOR[SLOT_COUNT] = {
        float3(0.90, 0.25, 0.25),
        float3(0.25, 0.85, 0.35),
        float3(0.35, 0.50, 0.95),
        float3(0.95, 0.85, 0.25)
    };

    DrawData d = LoadDraw(drawID);
    // the whole point of this spike: the pool offset travels in the draw row,
    // never in the args' baseVertex
    Vertex input = LoadVertex(d, vertexID);

    VertexOut output;
    output.posProj = mul(viewProj, mul(d.world, float4(input.position, 1.0)));
    output.color = float4(SLOT_COLOR[min(d.objectID, SLOT_COUNT - 1)], 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
