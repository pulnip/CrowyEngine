#include "SceneData.slang"

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
    DrawData d = LoadDraw(drawID);
    Vertex input = LoadVertex(d, vertexID);

    VertexOut output;
    output.posProj = mul(viewProj, mul(d.world, float4(input.position, 1.0)));
    output.normal = mul((float3x3)d.world, input.normal);
    output.materialIndex = d.materialIndex;

    return output;
}

static const float3 LightDir = normalize(float3(0.4, 0.8, -0.45));

float3 Shade(VertexOut input){
    MaterialData m = LoadMaterial(input.materialIndex);

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
