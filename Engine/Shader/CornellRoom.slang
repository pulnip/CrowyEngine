// mirror of DrawData / MaterialData / ScenePush
struct DrawData{
    float4x4 world;
    uint materialIndex;
    uint objectID;
    uint vbIndex;
    uint _pad0;
};

struct MaterialData{
    float3 albedo;
    float metallic;
    float3 emissive;
    float roughness;
    uint albedoMapID;
    uint normalMapID;
    uint mrMapID;
    uint _pad0;
};

struct ScenePush{
    DescriptorHandle<StructuredBuffer<DrawData>> draws;
    DescriptorHandle<StructuredBuffer<MaterialData>> materials;
};

[[vk::push_constant]]
ConstantBuffer<ScenePush> pass : register(b0);

// mirror of ViewData's prefix
cbuffer view : register(b1) {
    float4x4 viewProj;
};

cbuffer light : register(b2) {
    // xyz = camera position, w unused
    float4 cameraPos_;
    // xyz = light center, w = light area
    float4 lightCenter_lightArea;
    // xyz = light normal, w unused
    float4 lightNormal_;
    // xyz = light radiance, w = ambient strength
    float4 lightRadiance_ambient;
};

struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent : TANGENT;
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
    VertexIn input,
    uint drawID : SV_StartInstanceLocation
){
    StructuredBuffer<DrawData> draws = pass.draws;
    DrawData d = draws[drawID];

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

static const float PI = 3.14159265;

float3 fresnelSchlick(float3 f0, float cosTheta) {
    return f0 + (1.0 - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float distributionGGX(float NoH, float a) {
    float a2 = a * a;
    float d = NoH * NoH * (a2 - 1.0) + 1.0;

    return a2 / max(PI * d * d, 1e-6);
}

// Smith height-correlated, already carrying the 1/(4*NoL*NoV) of the BRDF
float visibilitySmith(float NoV, float NoL, float a) {
    float a2 = a * a;
    float v = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float l = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);

    return 0.5 / max(v + l, 1e-6);
}

[shader("fragment")]
float4 fs_main(VertexOut input) : SV_Target {
    float3 cameraPos = cameraPos_.xyz;
    float3 lightCenter = lightCenter_lightArea.xyz;
    float lightArea = lightCenter_lightArea.w;
    float3 lightNormal = lightNormal_.xyz;
    float3 lightRadiance = lightRadiance_ambient.xyz;
    float ambient = lightRadiance_ambient.w;

    StructuredBuffer<MaterialData> materials = pass.materials;
    MaterialData m = materials[input.materialIndex];

    float3 albedo = m.albedo;
    float metallic = m.metallic;
    float3 emissive = m.emissive;
    float roughness = m.roughness;

    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.posWorld);

    // the area light is collapsed to its center and weighted by its area.
    // that is wrong close to the light and close enough further away,
    // which is all a rasterized sample needs.
    // a ground truth version of this scene
    // wants a path tracer, and this is deliberately not one
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
