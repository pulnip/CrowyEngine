#pragma once

// What every Render shader reads, mirrored from the C++ side: DrawData,
// ViewData and ScenePush in RenderSceneData.hpp, MaterialData in
// RenderMaterial.hpp, LightData in RenderLight.hpp, Vertex in
// Engine/RHI/Common/Vertex.hpp.

// a structured buffer packs tightly
struct Vertex{
    float3 position;
    float3 normal;
    float2 texCoord;
    float4 tangent;
};

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

struct LightData{
    float4x4 worldToShadow;
    float3 position;
    float invRange;
    // unit, the way the light travels
    float3 direction;
    uint kind;
    // colour x intensity
    float3 color;
    uint shadowIndex;
    float coneScale;
    float coneOffset;
    float shadowBias;
    float shadowNormalBias;
};

// per-draw data rides in draws[drawID], never here
struct ScenePush{
    DescriptorHandle<StructuredBuffer<DrawData>> draws;
    DescriptorHandle<StructuredBuffer<MaterialData>> materials;
    DescriptorHandle<StructuredBuffer<Vertex>> vertices;
    // row 0 of this frame's slice inside the shared transient buffer
    uint drawBase;
    uint materialBase;
    DescriptorHandle<StructuredBuffer<LightData>> lights;
    uint lightBase;
    uint lightCount;
    // the directional shadow map, in a pass that reads it
    DescriptorHandle<Texture2D<float>> shadowMap;
};

[[vk::push_constant]]
ConstantBuffer<ScenePush> pass : register(b0);

// scalars and float4 only, so HLSL and Metal lay it out alike
cbuffer view : register(b1) {
    float4x4 viewProj;
    uint debugMode;
    // a ShadowFilter
    uint shadowFilter;
    uint _pad1;
    uint _pad2;
    // xyz = camera position, w unused
    float4 cameraPosition;
    // xyz, w unused; equal colours are a flat ambient
    float4 skyAmbient;
    float4 groundAmbient;
};

DrawData LoadDraw(uint drawID){
    StructuredBuffer<DrawData> draws = pass.draws;

    return draws[pass.drawBase + drawID];
}

// SV_VertexID is the raw index value: Metal folds the draw's baseVertex into
// it and D3D12 does not, so the pool row comes from the draw instead.
// VertexPullSpike is the regression check for this contract.
Vertex LoadVertex(DrawData draw, uint vertexID){
    StructuredBuffer<Vertex> vertices = pass.vertices;

    return vertices[draw.vbIndex + vertexID];
}

MaterialData LoadMaterial(uint materialIndex){
    StructuredBuffer<MaterialData> materials = pass.materials;

    return materials[pass.materialBase + materialIndex];
}
