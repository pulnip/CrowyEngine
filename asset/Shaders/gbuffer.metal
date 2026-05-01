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

vertex VertexOut vs_gbuffer(
    VertexIn in                 [[ stage_in]],
    constant Uniforms& uniforms [[buffer(1)]]
){
    VertexOut out;
    out.position = uniforms.mvp * float4(in.position, 1.0);
    out.normal   = in.normal;
    out.texCoord = in.texCoord;
    return out;
}

struct GBufferOut{
    float4 albedo [[color(0)]];
    float4 normal [[color(1)]];
};

fragment GBufferOut fs_gbuffer(
    VertexOut             in [[  stage_in]],
    texture2d<float> diffuse [[texture(0)]],
    sampler                s [[sampler(0)]]
){
    GBufferOut out;

    // no lighting
    float4 dif = diffuse.sample(samp, in.texCoord);
    out.albedo = float4(dif.rgb, 1.0);

    // encode to [0, 1]
    float3 n = normalize(in.normal);
    out.normal = float4(0.5 * n + 0.5, 1.0);

    return out;
}
