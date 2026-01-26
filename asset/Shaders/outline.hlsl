struct VertexOut{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

Texture2D    normalTex : register(t0);
Texture2D    depthTex  : register(t1);
Texture2D    colorTex  : register(t2);
SamplerState s         : register(s0);

float4 fs_outline(VertexOut input) : SV_Target{
    // TODO
    float2 texelSize = float2(1.0/800, 1.0/600);

    static const float2 offsets[8] = {
        float2(-1, -1), float2(0, -1), float2(1, -1),
        float2(-1,  0),                float2(1,  0),
        float2(-1,  1), float2(0,  1), float2(1,  1)
    };

    float3 normalCenter = normalTex.Sample(s, input.texCoord).rgb;
    float normalEdge = 0.0;
    for(int i=0; i<8; ++i){
        float2 sampleCoord = input.texCoord + texelSize * offsets[i];

        float3 normalSample = normalTex.Sample(s, sampleCoord).rgb;
        normalEdge += distance(normalCenter, normalSample);
    }

    float depthCenter = depthTex.Sample(s, input.texCoord).r;
    float depthEdge = 0.0;
    for(int i=0; i<8; ++i){
        float2 sampleCoord = input.texCoord + texelSize * offsets[i];

        float depthSample = depthTex.Sample(s, sampleCoord).r;
        depthEdge += abs(depthCenter - depthSample);
    }

    // float edge = saturate(2.0*normalEdge + 50.0*depthEdge);
    float edge = saturate(50.0*depthEdge);
    // thresholding
    edge = step(0.1, edge);

    float3 color = colorTex.Sample(s, input.texCoord).rgb;
    float3 outlineColor = float3(0.0, 0.0, 0.0);

    color = lerp(color, outlineColor, edge);

    return float4(color, 1.0);
}