#include "Sampler.slang"

// Terrain surface shader shared by both samples. No input layout:
// vertices are pulled from a structured buffer by SV_VertexID.

// Mirrors TerrainVertex in Engine/RHI/Sample/Terrain/Terrain.hpp.
struct TerrainVertex {
    float3 position;
    float3 normal;
};

struct ResourceData {
    // Vertex is written by GPU, so use structured buffer instead input layout
    DescriptorHandle<StructuredBuffer<TerrainVertex>> vertices;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource : register(b0);

cbuffer frame : register(b1) {
    float4x4 viewProj;
    float4 toLight_ambient;
};

struct VertexOut {
    float4 posProj : SV_Position;
    // triplanar: world position stands in for UVs
    float3 posWorld : TEXCOORD0;
    float3 normal : NORMAL;
};

[shader("vertex")]
VertexOut vs_main(uint vertexID : SV_VertexID) {
    StructuredBuffer<TerrainVertex> vertices = resource.vertices;
    TerrainVertex vertex = vertices[vertexID];

    VertexOut output;
    output.posProj = mul(viewProj, float4(vertex.position, 1.0));
    output.posWorld = vertex.position;
    output.normal = vertex.normal;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    float3 N = normalize(input.normal);

    // placeholder flat albedo until triplanar + slope blend land
    float3 albedo = float3(0.62, 0.60, 0.56);

    float3 toLight = normalize(toLight_ambient.xyz);
    float ambient = toLight_ambient.w;
    float ndotl = saturate(dot(N, toLight));

    return float4(albedo * (ambient + ndotl), 1.0);
}

[shader("fragment")]
float4 fs_debug_normal(VertexOut input) : SV_Target {
    float3 N = normalize(input.normal);

    return float4(0.5 * N + 0.5, 1);
}
