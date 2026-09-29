#include "DebugView.slang"
#include "Lighting.slang"
#include "SceneData.slang"

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
    DrawData d = LoadDraw(drawID);
    Vertex input = LoadVertex(d, vertexID);

    float4 posWorld = mul(d.world, float4(input.position, 1.0));

    VertexOut output;
    output.posProj = mul(viewProj, posWorld);
    output.posWorld = posWorld.xyz;
    // every scale in the map is uniform or stretches a box along its face
    // normals, so the scaled normal keeps its direction and the FS normalizes
    output.normal = mul((float3x3)d.world, input.normal);
    output.materialIndex = d.materialIndex;

    return output;
}

static const float PI = 3.14159265;

static const float GridMajor = 1.0;
static const float GridMinor = 0.25;

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

struct Surface {
    float3 albedo;
    float metallic;
    float3 emissive;
    float roughness;
    float3 normal;
};

Surface loadSurface(VertexOut input) {
    MaterialData m = LoadMaterial(input.materialIndex);

    Surface s;
    s.albedo = m.albedo;
    s.metallic = m.metallic;
    s.emissive = m.emissive;
    s.roughness = m.roughness;
    s.normal = normalize(input.normal);

    return s;
}

// one light's GGX; the radiance arrives un-cosined
float3 shadeLight(Surface s, LightSample light, float3 V) {
    float3 N = s.normal;
    float3 L = light.direction;

    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V)) + 1e-5;

    float3 H = normalize(L + V);
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    float a = max(s.roughness * s.roughness, 1e-3);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), s.albedo, s.metallic);
    float3 F = fresnelSchlick(f0, VoH);

    float3 specular = F * distributionGGX(NoH, a) * visibilitySmith(NoV, NoL, a);
    float3 diffuse = (1.0 - F) * (1.0 - s.metallic) * s.albedo / PI;

    return (diffuse + specular) * light.radiance * NoL * light.attenuation;
}

// the lights, then the ambient, then the emission
float3 shade(Surface s, float3 posWorld) {
    float3 V = normalize(cameraPosition.xyz - posWorld);

    float3 color = float3(0.0, 0.0, 0.0);
    // never early-outs: a shading model may tint what the shadow darkens
    for(uint i = 0; i < LightCount(); ++i)
        color += shadeLight(s, SampleLight(i, posWorld, s.normal), V);
    color += AmbientIrradiance(s.normal) * s.albedo;
    color += s.emissive;

    return color;
}

// the lit picture as scene radiance, or the debug view the frame asks for;
// the post list takes both to the screen
float3 present(Surface s, VertexOut input) {
    // the view is uniform, so only the Shadow view walks the lights twice
    float shadow = 1.0;
    if(debugMode == DebugModeShadow)
        shadow = ShadowTerm(input.posWorld, s.normal);

    return ApplyDebugView(
        debugMode,
        shade(s, input.posWorld),
        s.albedo + s.emissive,
        s.normal,
        input.posProj.z,
        shadow
    );
}

// 1 where a line of this spacing crosses the pixel, fading out before the
// cells shrink below a few pixels and turn to noise
float gridLine(float2 coord, float2 footprint, float spacing) {
    float2 toLine = abs(frac(coord / spacing - 0.5) - 0.5) * spacing;
    float2 pixels = toLine / max(footprint, 1e-5);
    float cellPixels = spacing / max(max(footprint.x, footprint.y), 1e-5);

    return (1.0 - saturate(min(pixels.x, pixels.y))) *
        saturate((cellPixels - 2.0) / 4.0);
}

// How much a world-aligned grid darkens the albedo here, drawn in the plane
// facing the normal's dominant axis, so floors and walls both carry it
float gridDarkening(float3 posWorld, float3 N) {
    // derivatives first, in uniform control flow: inside a branch the
    // neighbouring pixels might not take, they are undefined. Scalar fwidth,
    // because the vector overload does not target Metal
    float3 footprint = float3(
        fwidth(posWorld.x),
        fwidth(posWorld.y),
        fwidth(posWorld.z)
    );

    float3 a = abs(N);
    float2 coord = posWorld.xy;
    float2 width = footprint.xy;
    if(a.y >= a.x && a.y >= a.z) {
        coord = posWorld.xz;
        width = footprint.xz;
    } else if(a.x >= a.z) {
        coord = posWorld.zy;
        width = footprint.zy;
    }

    float major = gridLine(coord, width, GridMajor);
    float minor = gridLine(coord, width, GridMinor);

    return max(0.55 * major, 0.25 * minor);
}

[shader("fragment")]
float4 fs_grid(VertexOut input) : SV_Target {
    Surface s = loadSurface(input);
    s.albedo *= 1.0 - gridDarkening(input.posWorld, s.normal);

    return float4(present(s, input), 1.0);
}

[shader("fragment")]
float4 fs_opaque(VertexOut input, bool frontFace : SV_IsFrontFace) : SV_Target {
    Surface s = loadSurface(input);
    // a double-sided material lights its back as a front; a single-sided
    // one culls its back, so this never fires for it
    if(!frontFace)
        s.normal = -s.normal;

    return float4(present(s, input), 1.0);
}

// A second entry point rather than an alpha channel in MaterialData
[shader("fragment")]
float4 fs_translucent(VertexOut input) : SV_Target {
    Surface s = loadSurface(input);

    return float4(present(s, input), 0.45);
}
