#pragma once

// The engine's forward loop: include it, write a fragment entry, and link a
// shading model that exports MaterialShading.

#include "SceneData.slang"
#include "DebugView.slang"
#include "Lighting.slang"
import Shading;

// a linked module's export replaces the default
extern struct MaterialShading : IShadingModel = UnlinkedShading;

struct VertexOut {
    float4 posProj : SV_Position;
    float3 posWorld : POSITION;
    float3 normal : NORMAL;
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
    output.materialIndex = d.materialIndex;

    return output;
}

// the material row under this fragment, facing the viewer
Surface loadSurface(VertexOut v, bool frontFace) {
    MaterialData m = LoadMaterial(v.materialIndex);

    Surface s;
    s.albedo = m.albedo;
    s.metallic = m.metallic;
    s.emissive = m.emissive;
    s.roughness = m.roughness;
    s.normal = normalize(v.normal);
    // a double-sided material lights its back as a front; a single-sided
    // one culls its back, so this never fires for it
    if(!frontFace)
        s.normal = -s.normal;
    s.opacity = m.opacity;
    s.custom0 = m.custom0;
    s.custom1 = m.custom1;

    return s;
}

// the lights through the linked model, then its ambient and its composition
float3 shadeSurface(Surface s, float3 posWorld) {
    float3 V = normalize(cameraPosition.xyz - posWorld);

    LightSum sum = {};
    // never early-outs: a model may tint what the shadow darkens
    for(uint i = 0; i < LightCount(); ++i)
        MaterialShading.accumulate(s, SampleLight(i, posWorld, s.normal), V, sum);

    return MaterialShading.compose(
        s,
        sum,
        MaterialShading.ambient(s, V, AmbientIrradiance(s.normal))
    );
}

// the fragment's scene radiance, or the debug view the frame asks for; the
// post list takes both to the screen
float3 sceneColor(Surface s, VertexOut v) {
    // the view is uniform, so only the Shadow view walks the lights twice
    float shadow = 1.0;
    if(debugMode == DebugModeShadow)
        shadow = ShadowTerm(v.posWorld, s.normal);

    return ApplyDebugView(
        debugMode,
        shadeSurface(s, v.posWorld),
        s.albedo + s.emissive,
        s.normal,
        v.posProj.z,
        shadow
    );
}
