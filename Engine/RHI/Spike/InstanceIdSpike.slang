// Slang's Metal target emits [[instance_id]] raw, counting from the base
// instance, while D3D12 keeps SV_InstanceID zero-based.
// PASS: three left-aligned rows of four quads, one row per draw.
// FAIL: a staircase - row 2 starting at column 2, row 3 at column 3.

#include "InstanceIndex.slang"

static const uint ROW_COUNT = 3;
static const uint COLUMN_SLOTS = 8;
static const uint INSTANCES_PER_ROW = 4;

struct VertexOut{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID,
    uint drawID : SV_StartInstanceLocation
){
    static const float2 corners[4] = {
        float2(0, 0),
        float2(1, 0),
        float2(0, 1),
        float2(1, 1)
    };
    static const float3 rowColor[ROW_COUNT] = {
        float3(1, 0.3, 0.3),
        float3(0.3, 1, 0.3),
        float3(0.3, 0.5, 1)
    };

    // draws start at baseInstance 1, so a base instance that never arrives
    // cannot pass for row 0
    const uint row = drawID - 1;
    const uint column = zeroBasedInstanceID(instanceID, drawID);

    const float cellW = 2.0 / COLUMN_SLOTS;
    const float cellH = 2.0 / ROW_COUNT;
    const float margin = 0.12;

    const float2 corner = corners[vertexID];
    const float left = -1.0 + (column + margin) * cellW;
    const float right = -1.0 + (column + 1.0 - margin) * cellW;
    const float top = 1.0 - (row + margin) * cellH;
    const float bottom = 1.0 - (row + 1.0 - margin) * cellH;

    VertexOut output;
    output.position = float4(
        lerp(left, right, corner.x),
        lerp(bottom, top, corner.y),
        0, 1
    );
    // brightness ramps across the row, so a shifted block is readable even
    // where two rows overlap
    output.color = float4(
        rowColor[min(row, ROW_COUNT - 1)] *
            lerp(0.45, 1.0, float(column) / float(INSTANCES_PER_ROW - 1)),
        1
    );

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
