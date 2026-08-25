// mirror of Vertex in Engine/RHI/Common/Vertex.hpp.
// A structured buffer pack tightly
struct Vertex{
    float3 position;
    float3 normal;
    float2 texCoord;
    float4 tangent;
};

// mirror of DrawData / MaterialData / ScenePush
struct DrawData{
    float4x4 world;
    uint materialIndex;
    uint objectID;
    uint vbIndex;
    uint _pad0;
};

struct MaterialData{
    float3 albedo;
    float metallic;
    float3 emissive;
    float roughness;
    uint albedoMapID;
    uint normalMapID;
    uint mrMapID;
    uint _pad0;
};

struct ScenePush{
    DescriptorHandle<StructuredBuffer<DrawData>> draws;
    DescriptorHandle<StructuredBuffer<MaterialData>> materials;
    DescriptorHandle<StructuredBuffer<Vertex>> vertices;
    // row 0 of this frame's slice inside the shared transient buffer
    uint drawBase;
    uint materialBase;
};

[[vk::push_constant]]
ConstantBuffer<ScenePush> pass : register(b0);

cbuffer view : register(b1) {
    float4x4 viewProj;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 normal : NORMAL;
    nointerpolation uint materialIndex : MATERIALINDEX;
};

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    StructuredBuffer<DrawData> draws = pass.draws;
    DrawData d = draws[pass.drawBase + drawID];

    // SV_VertexID is the raw index value: Metal folds the draw's baseVertex
    // into it and D3D12 does not, so the pool row comes from the draw instead.
    StructuredBuffer<Vertex> vertices = pass.vertices;
    Vertex input = vertices[d.vbIndex + vertexID];

    VertexOut output;
    output.posProj = mul(viewProj, mul(d.world, float4(input.position, 1.0)));
    output.normal = mul((float3x3)d.world, input.normal);
    output.materialIndex = d.materialIndex;

    return output;
}

static const float3 LightDir = normalize(float3(0.4, 0.8, -0.45));

float3 Shade(VertexOut input){
    StructuredBuffer<MaterialData> materials = pass.materials;
    MaterialData m = materials[pass.materialBase + input.materialIndex];

    // two-sided lambert.
    float3 n = normalize(input.normal);
    float lambert = abs(dot(n, LightDir));

    return m.albedo * (0.25 + 0.75 * lambert) + m.emissive;
}

[shader("fragment")]
float4 fs_opaque(VertexOut input) : SV_Target {
    return float4(Shade(input), 1.0);
}

// A second entry point rather than an alpha channel in MaterialData
[shader("fragment")]
float4 fs_translucent(VertexOut input) : SV_Target {
    return float4(Shade(input), 0.45);
}
