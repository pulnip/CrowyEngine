#pragma once

#include <cstddef>
#include <type_traits>

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

// Everything the GPU reads.
namespace Crowy
{
    // the RHI slot a pass binds its ViewData row at: `view`, `b1`
    inline constexpr u32 ViewConstantBufferSlot = 0;

    // One row per draw, so a mesh's submeshes duplicate `world` between them.
    // That is what lets baseInstance be the row index with nothing in between.
    struct DrawData {
        Mat4 world = unitMat();
        u32 materialIndex = 0;
        // survives culling, unlike drawID
        // drawID와 달리 컬링 후에도 살아남는 식별자
        u32 objectID = 0;
        // first pool row of this mesh's vertices: the draws pass baseVertex 0,
        // because SV_VertexID adds it on Metal but not on D3D12
        u32 vbIndex = 0;
        u32 _pad0 = 0;
    };
    static_assert(sizeof(DrawData) == 80);
    static_assert(offsetof(DrawData, materialIndex) == 64);
    static_assert(offsetof(DrawData, objectID) == 68);
    static_assert(offsetof(DrawData, vbIndex) == 72);
    static_assert(std::is_trivially_copyable_v<DrawData>);

    // Only scalars and 16-byte vectors: HLSL packs a float3 after a scalar
    // tighter than Metal does.
    struct ViewData {
        Mat4 viewProj = unitMat();
        // which debug view a shader that branches on it draws; 0 is none
        u32 debugMode = 0;
        // a ShadowFilter
        u32 shadowFilter = 0;
        u32 _pad0[2]{};
        // xyz = camera position, w unused
        Vec4 cameraPosition{};
        // xyz, w unused: the scene's hemisphere, sky facing up and ground
        // facing down; equal colours are a flat ambient
        Vec4 skyAmbient{};
        Vec4 groundAmbient{};

        f32 _pad[32]{};
    };
    static_assert(sizeof(ViewData) == RHI_CB_ALIGN);
    static_assert(offsetof(ViewData, debugMode) == 64);
    static_assert(offsetof(ViewData, shadowFilter) == 68);
    static_assert(offsetof(ViewData, cameraPosition) == 80);
    static_assert(offsetof(ViewData, skyAmbient) == 96);
    static_assert(offsetof(ViewData, groundAmbient) == 112);
    static_assert(std::is_trivially_copyable_v<ViewData>);

    // A shader wanting more declares a struct beginning with these members,
    // so `draws` keeps its offset.
    struct ScenePush {
        // DescriptorHandle<StructuredBuffer<DrawData>>
        u64 draws = 0;
        // DescriptorHandle<StructuredBuffer<MaterialData>>
        u64 materials = 0;
        // DescriptorHandle<StructuredBuffer<Vertex>>, indexed by SV_VertexID
        u64 vertices = 0;
        // `draws` and `materials` name one descriptor over storage many
        // frames share, so row 0 of this frame's slice sits at these offsets
        u32 drawBase = 0;
        u32 materialBase = 0;
        // DescriptorHandle<StructuredBuffer<LightData>>, over the same
        // shared storage, so lightBase is its row 0
        u64 lights = 0;
        u32 lightBase = 0;
        u32 lightCount = 0;
        // DescriptorHandle<Texture2D<float>>: the directional shadow map, for
        // a pass that reads it; 0 otherwise
        u64 shadowMap = 0;
        // DescriptorHandle<StructuredBuffer<TextureData>>: the frame's texture
        // table, whose row 0 sits at textureBase; 0 with no texture
        u64 textures = 0;
        u32 textureBase = 0;
        u32 _pad = 0;
    };
    static_assert(sizeof(ScenePush) == 72);
    static_assert(offsetof(ScenePush, materials) == 8);
    static_assert(offsetof(ScenePush, vertices) == 16);
    static_assert(offsetof(ScenePush, drawBase) == 24);
    static_assert(offsetof(ScenePush, materialBase) == 28);
    static_assert(offsetof(ScenePush, lights) == 32);
    static_assert(offsetof(ScenePush, lightBase) == 40);
    static_assert(offsetof(ScenePush, lightCount) == 44);
    static_assert(offsetof(ScenePush, shadowMap) == 48);
    static_assert(offsetof(ScenePush, textures) == 56);
    static_assert(offsetof(ScenePush, textureBase) == 64);
    static_assert(sizeof(ScenePush) <= RHI_PUSH_CONSTANT_BYTES);
    static_assert(std::is_trivially_copyable_v<ScenePush>);

    // Every fullscreen pass's push: its reads in order, then params.
    // Mirrored in Engine/Shader/Fullscreen.slang.
    struct FullscreenPush {
        // DescriptorHandle<Texture2D> each; 0 for a read the pass lacks
        u64 source = 0;
        u64 input0 = 0;
        u64 input1 = 0;
        u64 input2 = 0;
        Vec4 params{};
    };
    static_assert(sizeof(FullscreenPush) == 48);
    static_assert(offsetof(FullscreenPush, input0) == 8);
    static_assert(offsetof(FullscreenPush, params) == 32);
    static_assert(sizeof(FullscreenPush) <= RHI_PUSH_CONSTANT_BYTES);
    static_assert(std::is_trivially_copyable_v<FullscreenPush>);
}
