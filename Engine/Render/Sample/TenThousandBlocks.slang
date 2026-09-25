#include "SceneData.slang"

struct VertexOut{
    float4 posProj : SV_Position;
    float4 color : COLOR0;
};

// keep in sync with TenThousandBlocks.cpp
static const uint3 GRID = uint3(20, 20, 25);

// Minecraft-style fixed brightness per face direction;
// keep constants so the capture check can predict exact pixel values
float FaceBrightness(float3 n){
    if(n.y > 0.5)       return 1.0;
    if(n.y < -0.5)      return 0.5;
    if(abs(n.x) > 0.5)  return 0.6;
    return 0.8;
}

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    DrawData d = LoadDraw(drawID);
    Vertex input = LoadVertex(d, vertexID);

    float4 posWorld = mul(d.world, float4(input.position, 1.0));

    // objectID, not drawID: culling compacts the visible list,
    // so drawID is a position in it rather than a lattice index.
    uint objectID = d.objectID;
    uint gx = objectID % GRID.x;
    uint gy = (objectID / GRID.x) % GRID.y;
    uint gz = objectID / (GRID.x * GRID.y);
    float3 base = float3(gx, gy, gz) / float3(GRID - uint3(1, 1, 1));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.color = float4(base * FaceBrightness(input.normal), 1.0);

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    return input.color;
}
