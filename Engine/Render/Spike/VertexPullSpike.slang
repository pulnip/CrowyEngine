// SV_VertexID on an indexed draw is the raw index on D3D12, while Metal's
// [[vertex_id]] adds baseVertex, so the pool offset rides in DrawData::vbIndex.
// PASS: 3, 4, 5, 6 sides left to right - red, green, blue, yellow.
// FAIL: any other shape - a draw read the pool from row 0.

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

    DrawData d = loadDraw(drawID);
    Vertex input = loadVertex(d, vertexID);

    VertexOut output;
    output.posProj = mul(viewProj, mul(d.world, float4(input.position, 1.0)));
    output.color = float4(SLOT_COLOR[min(d.objectID, SLOT_COUNT - 1)], 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
