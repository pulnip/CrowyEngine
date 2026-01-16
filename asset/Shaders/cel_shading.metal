#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

fragment float4 fs_cel_shading(
    VertexOut in               [[ stage_in ]],
    texture2d<float> albedoTex [[texture(0)]],
    texture2d<float> normalTex [[texture(1)]]
){
    constexpr sampler s(filter::linear);

    float3 albedo = albedoTex.sample(s, in.texCoord).rgb;
    float3 normal = normalTex.sample(s, in.texCoord).rgb * 2.0 - 1.0;

    // TODO. hard-coded light
    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float ndotl = max(dot(normal, lightDir), 0.0);

    // stepping
    float shadow = 0.3 +
                   0.4 * step(0.4, ndotl) +
                   0.4 * step(0.6, ndotl);

    float3 color = shadow * albedo;

    return float4(color, 1.0);
}