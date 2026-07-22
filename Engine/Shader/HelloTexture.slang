#include "Sampler.slang"
#include "SRGB.slang"

struct ResourceData {
    DescriptorHandle<Texture2D> diffuse;
    DescriptorHandle<Texture2D> normal;
    DescriptorHandle<Texture2D> bump;
};

[[vk::push_constant]]
ConstantBuffer<ResourceData> resource;

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
    Texture2D texture = resource.diffuse;

    return texture.Sample(linearWrap, input.uv);
}

[shader("fragment")]
float4 fs_normal(VertexOut input) : SV_Target {
    Texture2D texture = resource.normal;

    float2 rg = texture.Sample(linearWrap, input.uv).rg;
    // [0, 1] to [-1, 1]
    float2 xy = 2.0 * rg - 1.0;
    float z = sqrt(saturate(1.0 - dot(xy, xy)));
    float3 linear = 0.5 * float3(xy, z) + 0.5;

    // prevent app to display color space conversion
    return float4(srgb2linearApprox(linear), 1.0);
}

[shader("fragment")]
float4 fs_greyscale(VertexOut input) : SV_Target {
    Texture2D texture = resource.bump;

    float r = texture.Sample(linearWrap, input.uv).r;

    return float4(srgb2linearApprox(float3(r, r, r)), 1.0);
}
