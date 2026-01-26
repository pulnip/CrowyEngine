#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

struct PixelateParams{
    float2 resolution;
    float2 pixelSize;
};

fragment float4 fs_pixelate(
    VertexOut                 input [[ stage_in ]],
    texture2d<float>   sceneTexture [[texture(0)]],
    constant PixelateParams& params [[ buffer(0)]],
    sampler                       s [[sampler(0)]]
){
    float2 pixelCount = params.resolution / params.pixelSize;
    float2 quantizedUV = floor(input.texCoord * pixelCount) / pixelCount;
    // sample from centric point
    quantizedUV += 0.5 / pixelCount;

    return sceneTexture.sample(s, quantizedUV);
}