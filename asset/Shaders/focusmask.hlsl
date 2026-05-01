struct VertexOut{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

cbuffer params : register(b0){
    float2 focusCenter;
    float  focusRadius;
    float  falloff;
    float  aspectRatio;
};

float4 fs_focusmask(VertexOut input) : SV_Target{
    float2 diff = input.texCoord - focusCenter;
    // adjust for screen ratio
    diff.x *= aspectRatio;
    float dist = length(diff);

    // set  center of mask 1.0 for clearing
    // set outside of mask 0.0 for pixelize
    float mask = 1.0 - smoothstep(
        focusRadius,
        focusRadius + falloff,
        dist
    );

    return float4(mask, mask, mask, 1.0);
}