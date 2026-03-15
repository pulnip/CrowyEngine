struct VertexOut{
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

VertexOut vs_fullscreen(uint vertexID : SV_VertexID){
    // BL to TR convention
    static const float2 ndc[6] = {
        // BL triangle
        float2(-1, -1), float2( 1, -1), float2(-1,  1),
        // TR triangle
        float2(-1,  1), float2( 1, -1), float2( 1,  1)
    };
    // TL to BR convention
    static const float2 texCoords[6] = {
        // BL triangle
        float2(0, 1), float2(1, 1), float2(0, 0),
        // TR triangle
        float2(0, 0), float2(1, 1), float2(1, 0)
    };

    VertexOut output;
    output.position = float4(ndc[vertexID], 0, 1);
    output.texCoord = texCoords[vertexID];

    return output;
}

struct VertexOutNDC{
    float4 position : SV_Position;
    float2 ndc : TEXCOORD0;
};

VertexOutNDC vs_fullscreen_ndc(uint vertexID : SV_VertexID){
    // BL to TR convention
    static const float2 ndc[6] = {
        // BL triangle
        float2(-1, -1), float2( 1, -1), float2(-1,  1),
        // TR triangle
        float2(-1,  1), float2( 1, -1), float2( 1,  1)
    };

    VertexOutNDC output;
    output.position = float4(ndc[vertexID], 0, 1);
    output.ndc = ndc[vertexID];

    return output;
}

Texture2D tex : register(t0);

float4 fs_bypass(VertexOut input) : SV_Target{
    int2 coord = int2(input.position.xy);
    float4 sampled = tex.Load(int3(coord, 0));
    return float4(sampled.rgb, 1.0);
}