#include "Sampler.slang"
#include "Vertex.slang"

cbuffer uniform : register(b1) {
    float4x4 viewProj;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posModel : POSITION;
}

struct ResourceData {
    DescriptorHandle<TextureCube> texture;
    DescriptorHandle<StructuredBuffer<Vertex>> vertices;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource : register(b0);

[shader("vertex")]
VertexOut vs_main(uint vertexID : SV_VertexID) {
    StructuredBuffer<Vertex> vertices = resource.vertices;
    Vertex input = vertices[vertexID];

    VertexOut output;
    output.posProj = mul(viewProj, float4(input.position, 1.0));
    output.posModel = input.position;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    TextureCube texture = resource.texture;

    return texture.Sample(linearWrap, input.posModel);
}
