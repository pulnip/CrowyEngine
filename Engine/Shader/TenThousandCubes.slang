// mirror of DrawData in TenThousandCubes.cpp;
// change both sides together and keep the C++ static_assert(80B) green
struct DrawData{
    float4x4 world;
    uint materialIndex;
    uint objectID;
    uint vbIndex;
    uint _pad0;
};

// pass-scope only — per-draw data rides in draws[drawID], never here
struct PassData{
    DescriptorHandle<StructuredBuffer<DrawData>> draws;
};

[[vk::push_constant]]
ConstantBuffer<PassData> pass : register(b0);

cbuffer frame : register(b1) {
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

// keep in sync with TenThousandCubes.cpp
static const uint3 GRID = uint3(20, 20, 25);

// Minecraft-style fixed brightness per face direction; constants so the
// capture check can predict exact pixel values on camera-facing faces
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

    // color straight from the drawID channel (not from DrawData), so the
    // 3-axis gradient visually proves every draw received its own ID
    uint gx = drawID % GRID.x;
    uint gy = (drawID / GRID.x) % GRID.y;
    uint gz = drawID / (GRID.x * GRID.y);
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
