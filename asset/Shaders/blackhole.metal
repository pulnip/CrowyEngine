#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

struct BlackholeParams{
    float3 pos;
    float mass;
};

fragment float4 fs_blackhole(
    VertexOut                 input  [[stage_in ]],
    constant BlackholeParams& params [[buffer(0)]]
){
    return float4(params.pos, 1.0);
}
