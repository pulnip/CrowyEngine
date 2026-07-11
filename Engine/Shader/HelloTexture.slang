struct ResourceData {
    DescriptorHandle<Texture2D> texture;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;
// defined in Global RootSignature
[[vk::binding(1, 0)]]
SamplerState linearWrap : register(s0);

struct VertexOut{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

[shader("vertex")]
VertexOut vs_main(uint vertexID: SV_VertexID) {
    // TriangleStrip, Clockwise to Front-Face
    static const float2 ndc[4] = {
        float2(-1.0,  1.0), float2(1.0,  1.0),
        float2(-1.0, -1.0), float2(1.0, -1.0)
    };
    static const float2 texCoords[4] = {
        float2(0, 0), float2(1, 0),
        float2(0, 1), float2(1, 1)
    };

    VertexOut output;
    output.position = float4(ndc[vertexID], 0, 1);
    output.uv = texCoords[vertexID];

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    Texture2D texture = resource.texture;

    return texture.Sample(linearWrap, input.uv);
}
