#include "BRDF.slang"
#include "SceneData.slang"

cbuffer light : register(b2) {
    float4 cameraPos_;
    float4 lightCenter_lightArea;
    float4 lightNormal_;
    float4 lightRadiance_ambient;
};

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    // flat, because one draw is one material
    nointerpolation uint materialIndex : MATERIALINDEX;
};

[shader("vertex")]
VertexOut vs_main(
    uint vertexID : SV_VertexID,
    uint drawID : SV_StartInstanceLocation
){
    DrawData d = loadDraw(drawID);
    Vertex input = loadVertex(d, vertexID);

    float4 posWorld = mul(d.world, float4(input.position, 1.0));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.posWorld = posWorld.xyz;
    // the scene only translates its objects, so the upper 3x3 is orthonormal
    // and normals come through unchanged
    output.normal = mul((float3x3)d.world, input.normal);
    output.materialIndex = d.materialIndex;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    float3 cameraPos = cameraPos_.xyz;
    float3 lightCenter = lightCenter_lightArea.xyz;
    float lightArea = lightCenter_lightArea.w;
    float3 lightNormal = lightNormal_.xyz;
    float3 lightRadiance = lightRadiance_ambient.xyz;
    float ambient = lightRadiance_ambient.w;

    MaterialData m = loadMaterial(input.materialIndex);

    float3 albedo = m.albedo;
    float metallic = m.metallic;
    float3 emissive = m.emissive;
    float roughness = m.roughness;

    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.posWorld);

    // the area light collapses to its center, weighted by its area: wrong only
    // close to the light, and deliberately not a path tracer
    float3 toLight = lightCenter - input.posWorld;
    float dist2 = max(dot(toLight, toLight), 1e-4);
    float3 L = toLight * rsqrt(dist2);

    // emission leaves one face only, so nothing above the ceiling is lit
    float lightFacing = saturate(dot(-L, lightNormal));
    float3 radiance = lightRadiance * lightArea * lightFacing / dist2;

    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V)) + 1e-5;

    float3 H = normalize(L + V);
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float a = max(roughness * roughness, 1e-3);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = fresnelSchlick(f0, VoH);

    float3 specular = F * distributionGGX(NoH, a) * visibilitySmith(NoV, NoL, a);
    float3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;

    float3 color = (diffuse + specular) * radiance * NoL;
    // there is no bounce lighting here, so a flat term stands in for it and
    // keeps the unlit walls from going pure black
    color += ambient * albedo;
    color += emissive;

    // Reinhard, so the light quad does not blow out everything around it
    color = color / (1.0 + color);
    // the swapchain RTV is UNORM rather than sRGB, so encode by hand
    return float4(pow(color, 1.0 / 2.2), 1.0);
}
