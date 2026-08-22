// mirror of DrawData in Engine/Render/Public/RenderSceneData.hpp;
struct DrawData{
    float4x4 world;
    uint materialIndex;
    uint objectID;
    uint vbIndex;
    uint _pad0;
};

// mirror of ScenePush in the same header;
// per-draw data rides in draws[drawID], never here
struct ScenePush{
    DescriptorHandle<StructuredBuffer<DrawData>> draws;
};

[[vk::push_constant]]
ConstantBuffer<ScenePush> pass : register(b0);

// mirror of ViewData's prefix
cbuffer view : register(b1) {
    float4x4 viewProj;
};

struct VertexIn{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

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
    VertexIn input,
    uint drawID : SV_StartInstanceLocation
){
    StructuredBuffer<DrawData> draws = pass.draws;
    DrawData d = draws[drawID];

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
