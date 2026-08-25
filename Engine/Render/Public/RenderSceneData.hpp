#pragma once

#include <cstddef>
#include <type_traits>

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

// Everything the GPU reads.
namespace Crowy
{
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

    struct ViewData {
        Mat4 viewProj = unitMat();

        f32 _pad[48]{};
    };
    static_assert(sizeof(ViewData) == RHI_CB_ALIGN);
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
    };
    static_assert(sizeof(ScenePush) == 32);
    static_assert(offsetof(ScenePush, materials) == 8);
    static_assert(offsetof(ScenePush, vertices) == 16);
    static_assert(offsetof(ScenePush, drawBase) == 24);
    static_assert(offsetof(ScenePush, materialBase) == 28);
    static_assert(sizeof(ScenePush) <= RHI_PUSH_CONSTANT_BYTES);
    static_assert(std::is_trivially_copyable_v<ScenePush>);
}
