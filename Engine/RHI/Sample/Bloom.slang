#include "Sampler.slang"

struct BloomParam {
    DescriptorHandle<Texture2D> texture;
    float2 texelDir;
    float intensity;
};

[[vk::push_constant]]
ConstantBuffer<BloomParam> param;

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

float3 gaussian1dk9(
    float2 texCoord,
    float2 texelDir,
    Texture2D texture,
    SamplerState s
) {
    static const float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

    float2 texSize;
    texture.GetDimensions(texSize.x, texSize.y);

    float2 texelSize = 1.0 / texSize;
    float2 sizedDir = texelSize * texelDir;

    float3 sum = weights[0] * texture.Sample(s, texCoord).rgb;

    [unroll]
    for (int i = 1; i < 5; ++i) {
        float2 offset = i * sizedDir;
        sum += weights[i] * texture.Sample(s, texCoord - offset).rgb;
        sum += weights[i] * texture.Sample(s, texCoord + offset).rgb;
    }

    return sum;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    Texture2D texture = param.texture;
    SamplerState s = linearClamp;

    float4 center = texture.Sample(s, input.uv);

    // a zero step would make all 9 taps land on the same texel. used by the
    // upsample pass, which only wants the bilinear resample.
    if (all(param.texelDir == 0.0)) {
        return float4(param.intensity * center.rgb, center.a);
    }

    float3 bloom = gaussian1dk9(
        input.uv,
        param.texelDir,
        texture,
        s
    );

    return float4(param.intensity * bloom, center.a);
}
