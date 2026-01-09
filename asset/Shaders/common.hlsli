struct VertexIn{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 tangent  : TANGENT;
};

struct VertexOut{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
};