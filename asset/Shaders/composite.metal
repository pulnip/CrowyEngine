#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

fragment float4 fs_composite(
    VertexOut in            [[ stage_in ]],
    texture2d<float> origin [[texture(0)]],
    texture2d<float> masked [[texture(1)]],
    texture2d<float>   mask [[texture(2)]]
){
    constexpr sampler s(filter::linear);

    float4  start = origin.sample(s, in.texCoord);
    float4    end = masked.sample(s, in.texCoord);
    float4 factor =   mask.sample(s, in.texCoord);

    return mix(start, end, factor.r);
}