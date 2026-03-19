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

struct VertexOutNDC{
    float4 position [[position]];
    float2 ndc;
};

vertex VertexOutNDC vs_fullscreen_ndc(
    uint vertexID [[vertex_id]]
){
    // TL to BR convention
    float2 ndc[6] = {
        // BL triangle
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        // TR triangle
        float2(-1,  1), float2(1, -1), float2( 1, 1)
    };

    VertexOutNDC out;
    out.position = float4(ndc[vertexID], 0, 1);
    out.ndc = ndc[vertexID];

    return out;
}

struct VertexOutPolar{
    float4 position [[position]];
    float2 polarCoord;
};

vertex VertexOutPolar vs_fullscreen_polar(
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
    float2 polarCoord[6] = {
        // BL triangle
        float2(0, 2*M_PI_F), float2(1, 2*M_PI_F), float2(0, 0),
        // TR triangle
        float2(0, 0), float2(1, 2*M_PI_F), float2(1, 0)
    };

    VertexOutPolar out;
    out.position = float4(positions[vertexID], 0, 1);
    out.polarCoord = polarCoord[vertexID];

    return out;
}

fragment float4 fs_bypass(
    VertexOut      input [[ stage_in ]],
    texture2d<float> tex [[texture(0)]],
    sampler            s [[sampler(0)]]
){
    return tex.sample(s, input.texCoord);
}