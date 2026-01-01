#include <metal_stdlib>

using namespace metal;

struct Uniforms {
    float4x4 mvp;
};

struct VertexIn {
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
    float2 texCoord [[attribute(2)]];
    float4 tangent  [[attribute(3)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 normal;
    float2 texCoord;
};

vertex VertexOut vs_main(
    VertexIn in [[stage_in]],
    constant Uniforms& uniforms [[buffer(1)]]
){
    VertexOut out;
    out.position = uniforms.mvp * float4(in.position, 1.0);
    out.normal = in.normal;
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 fs_textured(
    VertexOut in [[stage_in]],
    texture2d<float> baseColorTex [[texture(0)]],
    sampler texSampler [[sampler(0)]]
){
    // Sample texture
    float4 texColor = baseColorTex.sample(texSampler, in.texCoord);

    // Simple lighting using normal
    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float ndotl = max(dot(in.normal, lightDir), 0.3);

    return float4(texColor.rgb * ndotl, texColor.a);
}

fragment float4 fs_unlit(VertexOut in [[stage_in]]){
    // Fallback: use normal as color
    float3 color = in.normal * 0.5 + 0.5;
    return float4(color, 1.0);
}
