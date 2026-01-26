#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

fragment float4 fs_composite(
    VertexOut         input [[ stage_in ]],
    texture2d<float> origin [[texture(0)]],
    texture2d<float> masked [[texture(1)]],
    texture2d<float>   mask [[texture(2)]],
    sampler               s [[sampler(0)]]
){
    float4  start = origin.sample(s, input.texCoord);
    float4    end = masked.sample(s, input.texCoord);
    float4 factor =   mask.sample(s, input.texCoord);

    return mix(start, end, factor.r);
}