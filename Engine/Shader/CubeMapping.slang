struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

cbuffer uniform : register(b1) {
    float4x4 viewProj;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posModel : POSITION;
}

[shader("vertex")]
VertexOut vs_main(VertexIn input) {
    VertexOut output;
    output.posProj = mul(viewProj, float4(input.position, 1.0));
    output.posModel = input.position;

    return output;
}

struct ResourceData {
    DescriptorHandle<TextureCube> texture;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource : register(b0);
[[vk::binding(1, 0)]]
SamplerState linearWrap : register(s0);

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    TextureCube texture = resource.texture;

    return texture.Sample(linearWrap, input.posModel);
}