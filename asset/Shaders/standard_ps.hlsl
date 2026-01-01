#include "common.hlsli"

Texture2D baseColorTex: register(t0);
SamplerState texSampler: register(s0);

float4 ps_textured(VertexOut input): SV_TARGET{
    // Sample texture
    float4 texColor = baseColorTex.Sample(texSampler, input.texCoord);

    // Simple lighting using normal
    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float ndotl = max(dot(input.normal, lightDir), 0.3);

    return float4(texColor.rgb * ndotl, texColor.a);
}

// Unlit fallback
float4 ps_unlit(VertexOut input): SV_TARGET{
    // use normal as color
    float3 color = input.normal * 0.5 + 0.5;
    return float4(color, 1.0);
}
