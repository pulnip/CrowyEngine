#include <metal_stdlib>

using namespace metal;

struct VertexOut{
    float4 position [[position]];
    float2 texCoord;
};

struct BlackholeParams{
    packed_float3 bhPos;
    float mass;
    packed_float3 camPos;
    float aspect;
    packed_float3 camRight;
    float tanHalfFov;
    float3 camUp;
    float3 camForward;
};

// [0, 1] to [-1, 1]
float2 uv2ndc(float2 uv){
    return float2(2.0 * uv.x - 1.0, 1.0 - 2.0 * uv.y);
}

// [-1, 1] to Image Plane
float2 ndc2ip(float2 ndc, float aspect, float tanHalfFov){
    return ndc * float2(aspect, 1.0) * tanHalfFov;
}

float3 ip2RayDir(float2 ip, float3 right, float3 up, float3 forward){
    return normalize(ip.x * right + ip.y * up + forward);
}

fragment float4 fs_blackhole(
    VertexOut                 input  [[stage_in ]],
    constant BlackholeParams& params [[buffer(0)]]
){
    float2 ip = ndc2ip(uv2ndc(input.texCoord), params.aspect, params.tanHalfFov);
    float3 dir = ip2RayDir(ip, params.camRight, params.camUp, params.camForward);

    float3 oc = params.camPos - params.bhPos;
    float b = dot(oc, dir);
    float rs = 5;
    float c = dot(oc, oc) - rs * rs;
    float disc = b * b - c;

    if(disc > 0.0 && (-b -sqrt(disc)) > 0)
        return float4(0, 0, 0, 1);
    return float4(dir * 0.5 + 0.5, 1.0);
}
