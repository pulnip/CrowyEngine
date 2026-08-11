#pragma once

#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "Terrain.hpp"

namespace Crowy
{
    // How the marching pass spends its vertices.
    //
    // Soup emits three per triangle and draws them unindexed - the simplest
    // thing that works, and what the non-indexed ExecuteIndirect path exists
    // for. Welded emits one per crossing grid edge and references it by index
    // instead. A closed surface carries about half as many vertices as
    // triangles, so the 3-per-triangle soup becomes 0.5, and the mesh lands at
    // roughly a third of the bytes even after paying for the indices.
    enum class TerrainMarchMode: u8{
        Soup,
        Welded
    };

    // Where a marching run leaves each buffer for whatever reads it next.
    // The defaults suit a sample that draws the result indirectly.
    struct TerrainMarchTargets{
        RHIResourceUsage vertices = RHIResourceUsage::SampledVertex;
        // nothing outside the compute passes reads it, but the release still
        // stands: next frame's clear is a write-after-write
        RHIResourceUsage counter = RHIResourceUsage::StorageCompute;
        RHIResourceUsage args = RHIResourceUsage::IndirectArgs;
        // Welded mode only; ignored by Soup
        RHIResourceUsage indices = RHIResourceUsage::IndexBuffer;
    };

    // The release halves a marching run emitted. The consuming pass has to
    // acquire with these same values - that is what pairs the two ends of an
    // edge.
    struct TerrainMarchEdges{
        RHIBufferBarrier vertices;
        RHIBufferBarrier counter;
        RHIBufferBarrier args;
        // Welded mode only; the draw acquires it as an index buffer
        RHIBufferBarrier indices;
    };

    // The GPU side of the terrain: density evaluation and marching cubes, both
    // compute. Nothing comes back to the CPU - the triangle count only ever
    // exists in a buffer.
    //
    // Owns its resources and the barrier bookkeeping between submissions, so a
    // sample only has to say what it wants to do with the results next.
    class TerrainMarcher{
    private:
        u32 triangleCapacity;

        RHIComputePipelineStateRAII clearPSO, densityPSO;
        RHIComputePipelineStateRAII marchPSO, argsPSO;
        RHIComputePipelineStateRAII edgesPSO, indicesPSO, argsIndexedPSO;

        RHITextureRAII densityTexture;
        RHIBufferRAII vertexBuffer;
        RHIBufferRAII counterBuffer;
        RHIBufferRAII triTableBuffer;
        RHIBufferRAII argsBuffer;
        RHIBufferRAII edgeVertexBuffer;
        RHIBufferRAII indexBuffer;
        RHIBufferRAII argsIndexedBuffer;

        // what the previous submission left each resource in.
        // Undefined until the first Record
        RHIResourceUsage densityResting = RHIResourceUsage::Undefined;
        RHIResourceUsage vertexResting = RHIResourceUsage::Undefined;
        RHIResourceUsage counterResting = RHIResourceUsage::Undefined;
        // the two argument buffers hold different layouts and are written by
        // different modes, so they age independently
        RHIResourceUsage argsResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsIndexedResting = RHIResourceUsage::Undefined;
        RHIResourceUsage indexResting = RHIResourceUsage::Undefined;
        RHIResourceUsage edgeVertexResting = RHIResourceUsage::Undefined;

    public:
        TerrainMarcher(RHIDevice&, u32 triangleCapacity);
        // the RAII members hold types this header only forward-declares
        ~TerrainMarcher();

        u32 TriangleCapacity() const noexcept{ return triangleCapacity; }
        u32 VertexCapacity() const noexcept{ return triangleCapacity * 3; }

        RHIBuffer& Vertices() noexcept{ return *vertexBuffer; }
        RHIBuffer& Counter() noexcept{ return *counterBuffer; }
        RHIBuffer& Indices() noexcept{ return *indexBuffer; }

        // the argument buffer the given mode fills; their layouts differ
        // (RHIDrawArgs against RHIDrawIndexedArgs), so they are separate
        RHIBuffer& Args(TerrainMarchMode mode) noexcept{
            return mode == TerrainMarchMode::Soup ? *argsBuffer : *argsIndexedBuffer;
        }

        // Records the density pass and the marching pass, leaving each buffer
        // where `targets` says. Re-recording is the normal case: the acquires
        // wind the buffers back from wherever the previous submission left
        // them, which is the write-after-read this sample exists to exercise.
        TerrainMarchEdges Record(
            RHICommandList&,
            const TerrainParams&,
            TerrainMarchMode = TerrainMarchMode::Soup,
            TerrainMarchTargets = {}
        );
    };

    // What the counter buffer holds once the marching pass has run.
    // Mirrors the four slots cs_clear zeroes in TerrainMarch.slang.
    struct TerrainMarchCounter{
        // reservations handed out, which runs past capacity once it overflows
        u32 triangleCount = 0;
        u32 overflowed = 0;
        // the first slot a dropped cell would have filled; the soup is
        // contiguous below it. All ones while nothing has been dropped
        u32 firstDropped = 0;
        // Welded mode only: vertices welded onto crossing edges. Soup leaves
        // it at zero, where triangleCount * 3 says it all
        u32 vertexCount = 0;

        // how much of the vertex buffer is actually a mesh
        u32 DrawableTriangles() const noexcept{
            return triangleCount < firstDropped ? triangleCount : firstDropped;
        }
    };
    static_assert(sizeof(TerrainMarchCounter) == 16);
}
