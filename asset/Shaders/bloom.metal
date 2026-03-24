#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

float3 gaussian1dk9(
    float2 texCoord, float2 direction,
    texture2d<float, access::sample> texture,
    sampler s
){
    float2 texelSize = 1.0 / float2(texture.get_width(), texture.get_height());
    float2 sizedDir = texelSize * direction;
    float weights[5] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};

    float3 result = weights[0] * texture.sample(s, texCoord).rgb;

    for(int i=1; i<5; ++i){
        float2 offset = i * sizedDir;
        result += weights[i] * texture.sample(s, texCoord + offset).rgb;
        result += weights[i] * texture.sample(s, texCoord - offset).rgb;
    }

    return result;
}

fragment float4 fs_horizontal_blur(
    VertexOut          input [[ stage_in ]],
    texture2d<float> texture [[texture(0)]],
    sampler                s [[sampler(0)]]
){
    float4 color = texture.sample(s, input.texCoord);
    return float4(gaussian1dk9(input.texCoord, float2(1, 0), texture, s), color.a);
}

fragment float4 fs_vertical_blur(
    VertexOut          input [[ stage_in ]],
    texture2d<float> texture [[texture(0)]],
    sampler                s [[sampler(0)]]
){
    float4 color = texture.sample(s, input.texCoord);
    return float4(gaussian1dk9(input.texCoord, float2(0, 1), texture, s), color.a);
}

fragment float4 fs_composite(
    VertexOut             input [[ stage_in ]],
    texture2d<float>   sceneTex [[texture(0)]],
    texture2d<float> bloom_1_2  [[texture(1)]],
    texture2d<float> bloom_1_4  [[texture(2)]],
    texture2d<float> bloom_1_8  [[texture(3)]],
    sampler                   s [[sampler(0)]]
){
    float3  color =   sceneTex.sample(s, input.texCoord).rgb;
    float3 b_1_2  =  bloom_1_2.sample(s, input.texCoord).rgb;
    float3 b_1_4  =  bloom_1_4.sample(s, input.texCoord).rgb;
    float3 b_1_8  =  bloom_1_8.sample(s, input.texCoord).rgb;

    float3 bloom = b_1_2 + b_1_4 + b_1_8;
    constexpr float intensity = 0.3;

    return float4(color + intensity * bloom, 1);
}
