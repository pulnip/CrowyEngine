struct Uniforms{
    float4x4 mvp;
};

cbuffer UniformBuffer : register(b1){
    Uniforms uniforms;
};

Texture2D    baseColorTex : register(t0);
SamplerState texSampler   : register(s0);

struct VertexIn{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent  : TANGENT;
};

struct VertexOut{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VertexOut vs_main(VertexIn input){
    VertexOut output;
    output.position = mul(uniforms.mvp, float4(input.position, 1.0));
    output.normal   = input.normal;
    output.texCoord = input.texCoord;
    return output;
}

float4 fs_textured(VertexOut input) : SV_TARGET{
    float4 texColor = baseColorTex.Sample(texSampler, input.texCoord);

    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float  ndotl    = max(dot(input.normal, lightDir), 0.3);

    return float4(texColor.rgb * ndotl, texColor.a);
}

float4 fs_unlit(VertexOut input) : SV_TARGET{
    float3 color = input.normal * 0.5 + 0.5;
    return float4(color, 1.0);
}