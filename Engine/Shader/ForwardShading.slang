#pragma once

// The engine's forward loop: include it, write a fragment entry, and link a
// shading model that exports MaterialShading.

#include "SceneData.slang"
#include "DebugView.slang"
#include "Lighting.slang"
#include "Sampler.slang"
import Shading;

// a linked module's export replaces the default
extern struct MaterialShading : IShadingModel = UnlinkedShading;

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    // flat, because one draw is one material
    nointerpolation uint materialIndex : MATERIALINDEX;
};

// the one position function every colour and depth program shares, which
// Equal behind the prepass rests on
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
    output.texCoord = input.texCoord;
    output.materialIndex = d.materialIndex;

    return output;
}

// a map's texel with the sampler its table row names; the row is one
// material's, so the branch is uniform across the draw
float4 SampleMap(uint mapID, float2 uv) {
    TextureData row = LoadTexture(mapID);
    Texture2D map = row.texture;

    switch(row.sampler) {
    case 0: return map.Sample(linearWrap, uv);
    case 1: return map.Sample(linearClamp, uv);
    case 2: return map.Sample(linearMirror, uv);
    case 3: return map.Sample(linearBorder, uv);
    case 4: return map.Sample(nearestWrap, uv);
    case 5: return map.Sample(nearestClamp, uv);
    case 6: return map.Sample(nearestMirror, uv);
    default: return map.Sample(nearestBorder, uv);
    }
}

// the material row under this fragment, its maps sampled, facing the viewer
Surface loadSurface(VertexOut v, bool frontFace) {
    MaterialData m = LoadMaterial(v.materialIndex);

    Surface s;
    s.albedo = m.albedo;
    s.metallic = m.metallic;
    s.emissive = m.emissive;
    s.roughness = m.roughness;
    s.opacity = m.opacity;

    float2 uv = v.texCoord * m.uvScaleOffset.xy + m.uvScaleOffset.zw;
    if(m.albedoMapID != 0) {
        float4 texel = SampleMap(m.albedoMapID, uv);
        s.albedo *= texel.rgb;
        s.opacity *= texel.a;
    }
    if(m.emissiveMapID != 0)
        s.emissive *= SampleMap(m.emissiveMapID, uv).rgb;

    s.normal = normalize(v.normal);
    // a double-sided material lights its back as a front; a single-sided
    // one culls its back, so this never fires for it
    if(!frontFace)
        s.normal = -s.normal;
    s.custom0 = m.custom0;
    s.custom1 = m.custom1;

    return s;
}

// the lights through the linked model, then its ambient and its
// composition; a surface that receives no shadow sees every light unshadowed
float3 shadeSurface(Surface s, float3 posWorld, bool receivesShadow) {
    float3 V = normalize(cameraPosition.xyz - posWorld);

    LightSum sum = {};
    // never early-outs: a model may tint what the shadow darkens
    for(uint i = 0; i < LightCount(); ++i) {
        LightSample light = receivesShadow
            ? SampleLight(i, posWorld, s.normal)
            : EvaluateLight(LoadLight(i), posWorld);
        MaterialShading.accumulate(s, light, V, sum);
    }

    return MaterialShading.compose(
        s,
        sum,
        MaterialShading.ambient(s, V, AmbientIrradiance(s.normal))
    );
}

float3 shadeSurface(Surface s, float3 posWorld) {
    return shadeSurface(s, posWorld, true);
}

// the fragment's scene radiance, or the debug view the frame asks for; the
// post list takes both to the screen
float3 sceneColor(Surface s, VertexOut v) {
    bool receivesShadow =
        (LoadMaterial(v.materialIndex).flags & MaterialFlagNoShadowReceive) == 0;

    // the view is uniform, so only the Shadow view walks the lights twice
    float shadow = 1.0;
    if(debugMode == DebugModeShadow && receivesShadow)
        shadow = ShadowTerm(v.posWorld, s.normal);

    return ApplyDebugView(
        debugMode,
        shadeSurface(s, v.posWorld, receivesShadow),
        s.albedo + s.emissive,
        s.normal,
        v.posProj.z,
        shadow
    );
}
