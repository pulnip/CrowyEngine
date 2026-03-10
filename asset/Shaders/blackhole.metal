#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

struct BlackholeParams{
    packed_float3 pos;
    float mass;
    packed_float3 camPos;
    float aspect;
    packed_float3 camRight;
    float tanHalfFov;
    float3 camUp;
    float3 camForward;
};

fragment float4 fs_blackhole(
    VertexOut                 input  [[stage_in ]],
    constant BlackholeParams& params [[buffer(0)]]
){
    return float4(params.camForward, 1.0);
}
