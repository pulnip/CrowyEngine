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
    float opacity;
    // two lanes the engine never reads
    float4 custom0;
    float4 custom1;
    // uv' = uv * xy + zw
    float4 uvScaleOffset;
    uint emissiveMapID;
    float alphaCutoff;
    uint flags;
    uint _pad;
};

// a row of the frame's texture table; map ID n reads row n - 1
struct TextureData{
    DescriptorHandle<Texture2D> texture;
    // a TextureSampler: StaticSamplers order
    uint sampler;
    uint _pad;
};

struct LightData{
    float4x4 worldToShadow;
    float3 position;
    float invRange;
    // unit, the way the light travels
    float3 direction;
    uint kind;
    // color x intensity
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
    DescriptorHandle<StructuredBuffer<TextureData>> textures;
    uint textureBase;
    uint _pad;
};

[[vk::push_constant]]
ConstantBuffer<ScenePush> pass : register(b0);

#include "View.slang"

DrawData loadDraw(uint drawID){
    StructuredBuffer<DrawData> draws = pass.draws;

    return draws[pass.drawBase + drawID];
}

// Metal folds the draw's baseVertex into SV_VertexID and D3D12 does not,
// so the pool row comes from the draw (VertexPullSpike checks it)
Vertex loadVertex(DrawData draw, uint vertexID){
    StructuredBuffer<Vertex> vertices = pass.vertices;

    return vertices[draw.vbIndex + vertexID];
}

MaterialData loadMaterial(uint materialIndex){
    StructuredBuffer<MaterialData> materials = pass.materials;

    return materials[pass.materialBase + materialIndex];
}

// a map ID's row; 0 names no texture and is never loaded
TextureData loadTexture(uint mapID){
    StructuredBuffer<TextureData> textures = pass.textures;

    return textures[pass.textureBase + mapID - 1];
}
