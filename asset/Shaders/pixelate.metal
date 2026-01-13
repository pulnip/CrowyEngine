#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

// no vertex buffer
vertex VertexOut vs_fullscreen(
    uint vertexID [[vertex_id]]
){
    // BL to TR convention
    float2 positions[6] = {
        // BL triangle
        float2(-1, -1), float2( 1, -1), float2(-1,  1),
        // TR triangle
        float2(-1,  1), float2( 1, -1), float2( 1,  1)
    };
    // TL to BR convention
    float2 texCoords[6] = {
        // BL triangle
        float2(0, 1), float2(1, 1), float2(0, 0),
        // TR triangle
        float2(0, 0), float2(1, 1), float2(1, 0)
    };

    VertexOut out;
    out.position = float4(positions[vertexID], 0, 1);
    out.texCoord = texCoords[vertexID];

    return out;
}

struct PixelateParams{
    float2 resolution;
    float2 pixelSize;
};

fragment float4 fs_pixelate(
    VertexOut in                    [[ stage_in ]],
    texture2d<float> sceneTexture   [[texture(0)]],
    constant PixelateParams& params [[ buffer(0)]]
){
    // point sampling
    constexpr sampler s(filter::nearest);

    float2 pixelCount = params.resolution / params.pixelSize;
    float2 quantizedUV = floor(in.texCoord * pixelCount) / pixelCount;
    // sample from centric point
    quantizedUV += 0.5 / pixelCount;

    return sceneTexture.sample(s, quantizedUV);
}