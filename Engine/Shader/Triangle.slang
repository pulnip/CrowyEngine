struct VertexOut{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

[shader("vertex")]
VertexOut vs_main(uint vertexID: SV_VertexID) {
    static const float2 ndc[3] = {
        float2(-0.4, -0.3),
        float2(0.0, 0.5),
        float2(0.4, -0.3)
    };
    static const float4 colors[3] = {
        float4(1, 0, 0, 1),
        float4(0, 1, 0, 1),
        float4(0, 0, 1, 1)
    };

    VertexOut output;
    output.position = float4(ndc[vertexID], 0, 1);
    output.color = colors[vertexID];

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
