static const uint DRAW_COUNT = 4;

struct VertexOut{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    static const float2 corners[4] = {
        float2(0, 0),
        float2(1, 0),
        float2(1, 1),
        float2(0, 1)
    };
    // index 0 must never appear; draws start at baseInstance 1
    static const float4 palette[DRAW_COUNT + 1] = {
        float4(0.2, 0.2, 0.2, 1),
        float4(1, 0, 0, 1),
        float4(0, 1, 0, 1),
        float4(0, 0, 1, 1),
        float4(1, 1, 1, 1)
    };

    const int column = int(drawID) - 1;
    const float columnWidth = 2.0 / DRAW_COUNT;
    const float margin = 0.1 * columnWidth;

    const float2 corner = corners[vertexID];
    const float left = -1.0 + column * columnWidth + margin;
    const float right = left + columnWidth - 2 * margin;

    VertexOut output;
    output.position = float4(
        lerp(left, right, corner.x),
        lerp(-0.5, 0.5, corner.y),
        0, 1
    );
    output.color = palette[min(drawID, DRAW_COUNT)];

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
