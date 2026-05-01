struct VertexOut{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

Texture2D    sceneTexture : register(t0);
// point sampling
SamplerState s            : register(s0);

cbuffer params : register(b0){
    float2 resolution;
    float2 pixelSize;
};

float4 fs_pixelate(VertexOut input) : SV_Target{
    float2 pixelCount = resolution / pixelSize;
    float2 quantizedUV = floor(input.texCoord * pixelCount) / pixelCount;
    // sample from centric point
    quantizedUV += 0.5 / pixelCount;

    return sceneTexture.Sample(s, quantizedUV);
}