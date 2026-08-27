// Does the engine's instance index restart at 0 in a draw whose baseInstance
// is not 0?
//
// D3D12 says yes: StartInstanceLocation feeds SV_StartInstanceLocation and
// leaves SV_InstanceID zero-based. Metal's [[instance_id]] instead counts from
// the base instance, and Slang's Metal target emits it raw - this spike drew
// the staircase on Metal until zeroBasedInstanceID papered over the gap.
// It now guards that contract: OrbitFrame builds its whole segment
// addressing on the zero-based reading, and one segment there is sub-pixel -
// it could never show this.
//
// Row comes from the draw (baseInstance, already proven by BaseInstanceSpike),
// column from the zero-based index. The helper working gives a left-aligned
// block on every backend; a base instance leaking through gives a staircase.

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

    // draws start at baseInstance 1, matching BaseInstanceSpike's convention
    // that index 0 must never appear
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
