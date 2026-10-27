#include "Sampler.slang"

struct CompositeParam {
    DescriptorHandle<Texture2D> scene;
    DescriptorHandle<Texture2D> bloom;
    float bloomStrength;
};

[[vk::push_constant]]
ConstantBuffer<CompositeParam> param;

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
    SamplerState s = linearClamp;
    float3 scene = param.scene.Sample(s, input.uv).rgb;
    float3 bloom = param.bloom.Sample(s, input.uv).rgb;

    // both are linear radiance and may well exceed 1
    float3 color = scene + param.bloomStrength * bloom;

    return float4(color, 1);
}
