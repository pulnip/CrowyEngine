#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

fragment float4 fs_outline(
    VertexOut            input [[ stage_in ]],
    texture2d<float> normalTex [[texture(0)]],
    texture2d<float>  depthTex [[texture(1)]],
    texture2d<float>  colorTex [[texture(2)]],
    sampler                  s [[sampler(0)]]
){
    // TODO
    float2 texelSize = float2(1.0/800, 1.0/600);

    float2 offsets[8] = {
        float2(-1, -1), float2(0, -1), float2(1, -1),
        float2(-1,  0),                float2(1,  0),
        float2(-1,  1), float2(0,  1), float2(1,  1)
    };

    float3 normalCenter = normalTex.sample(s, input.texCoord).rgb;
    float normalEdge = 0.0;
    for(int i=0; i<8; ++i){
        float2 sampleCoord = input.texCoord + texelSize * offsets[i];

        float3 normalSample = normalTex.sample(s, sampleCoord).rgb;
        normalEdge += distance(normalCenter, normalSample);
    }

    float depthCenter = depthTex.sample(s, input.texCoord).r;
    float depthEdge = 0.0;
    for(int i=0; i<8; ++i){
        float2 sampleCoord = input.texCoord + texelSize * offsets[i];

        float depthSample = depthTex.sample(s, sampleCoord).r;
        depthEdge += abs(depthCenter - depthSample);
    }

    // float edge = saturate(2.0*normalEdge + 50.0*depthEdge);
    float edge = saturate(50.0*depthEdge);
    // thresholding
    edge = step(0.1, edge);

    float3 color = colorTex.sample(s, input.texCoord).rgb;
    float3 outlineColor = float3(0.0, 0.0, 0.0);

    color = mix(color, outlineColor, edge);

    return float4(color, 1.0);
}