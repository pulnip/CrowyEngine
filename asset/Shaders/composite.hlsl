struct VertexOut{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

Texture2D    origin : register(t0);
Texture2D    masked : register(t1);
Texture2D    mask   : register(t2);
SamplerState s      : register(s0);

float4 fs_composite(VertexOut input) : SV_Target{
    float4  start = origin.Sample(s, input.texCoord);
    float4    end = masked.Sample(s, input.texCoord);
    float4 factor =   mask.Sample(s, input.texCoord);

    return lerp(start, end, factor.r);
}