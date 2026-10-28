#include "Sampler.slang"
#include "Vertex.slang"

cbuffer frame : register(b1) {
    float4x4 viewProj;
    float4x4 model;
    float4 toLight_ambient;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct ResourceData {
    DescriptorHandle<Texture2D> albedo;
    DescriptorHandle<StructuredBuffer<Vertex>> vertices;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource : register(b0);

[shader("vertex")]
VertexOut vs_main(uint vertexID : SV_VertexID) {
    StructuredBuffer<Vertex> vertices = resource.vertices;
    Vertex input = vertices[vertexID];

    float4 posWorld = mul(model, float4(input.position, 1.0));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.normal = mul((float3x3)model, input.normal);
    output.texCoord = input.texCoord;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    Texture2D albedoTex = resource.albedo;

    float3 toLight = toLight_ambient.xyz;
    float ambient = toLight_ambient.w;

    // the grid is the whole point of this scene, so it is sampled straight
    // with no parallax or normal mapping in the way
    float3 albedo = albedoTex.Sample(linearWrap, input.texCoord).rgb;

    float3 N = normalize(input.normal);
    float ndotl = saturate(dot(N, normalize(toLight)));

    return float4(albedo * (ambient + ndotl), 1.0);
}
