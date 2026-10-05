// The night sky behind everything: a triangle over the screen at the far
// plane, drawn where the scene left the depth clear.

#include "NightSky.slang"

struct SkyPush {
    // the camera's right and up scaled by the half-FOV tangents, and forward
    float4 right;
    float4 up;
    float4 forward;
    float4 toMoon;
};

[[vk::push_constant]]
ConstantBuffer<SkyPush> sky : register(b0);

struct SkyVertex {
    float4 position : SV_Position;
    float2 ndc : TEXCOORD0;
};

[shader("vertex")]
SkyVertex vs_sky(uint vertexID: SV_VertexID) {
    float2 ndc = float2(
        vertexID == 1u ? 3.0 : -1.0,
        vertexID == 2u ? 3.0 : -1.0
    );

    SkyVertex o;
    o.position = float4(ndc, 1.0, 1.0);
    o.ndc = ndc;
    return o;
}

[shader("fragment")]
float4 fs_sky(SkyVertex v) : SV_Target {
    float3 dir = normalize(
        sky.forward.xyz + sky.right.xyz * v.ndc.x + sky.up.xyz * v.ndc.y
    );
    return float4(nightSky(dir, sky.toMoon.xyz, true), 1.0);
}
