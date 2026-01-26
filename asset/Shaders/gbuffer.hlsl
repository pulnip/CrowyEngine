cbuffer Uniforms: register(b1){
    float4x4 mvp;
};

struct VertexIn{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent  : TANGENT;
};

struct VertexOut{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
};

VertexOut vs_gbuffer(VertexIn input){
    VertexOut output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.normal   = input.normal;
    output.texCoord = input.texCoord;
    return output;
}

Texture2D diffuse : register(t0);
SamplerState    s : register(s0);

struct GBufferOut{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
};

GBufferOut fs_gbuffer(VertexOut input){
    GBufferOut output;

    // no lighting
    float4 dif = diffuse.Sample(s, input.texCoord);
    output.albedo = float4(dif.rgb, 1.0);

    // encode to [0, 1]
    float3 n = normalize(input.normal);
    output.normal = float4(0.5 * n + 0.5, 1.0);

    return output;
}
