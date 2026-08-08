#pragma once

#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "Terrain.hpp"

namespace Crowy
{
    // Where a marching run leaves each buffer for whatever reads it next.
    // The defaults suit a sample that draws the result indirectly.
    struct TerrainMarchTargets{
        RHIResourceUsage vertices = RHIResourceUsage::SampledVertex;
        // nothing outside the compute passes reads it, but the release still
        // stands: next frame's clear is a write-after-write
        RHIResourceUsage counter = RHIResourceUsage::StorageCompute;
        RHIResourceUsage args = RHIResourceUsage::IndirectArgs;
    };

    // The release halves a marching run emitted. The consuming pass has to
    // acquire with these same values - that is what pairs the two ends of an
    // edge.
    struct TerrainMarchEdges{
        RHIBufferBarrier vertices;
        RHIBufferBarrier counter;
        RHIBufferBarrier args;
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

        RHIComputePipelineStateRAII clearPSO, densityPSO, marchPSO, argsPSO;

        RHITextureRAII densityTexture;
        RHIBufferRAII vertexBuffer;
        RHIBufferRAII counterBuffer;
        RHIBufferRAII triTableBuffer;
        RHIBufferRAII argsBuffer;

        // what the previous submission left each resource in.
        // Undefined until the first Record
        RHIResourceUsage densityResting = RHIResourceUsage::Undefined;
        RHIResourceUsage vertexResting = RHIResourceUsage::Undefined;
        RHIResourceUsage counterResting = RHIResourceUsage::Undefined;
        RHIResourceUsage argsResting = RHIResourceUsage::Undefined;

    public:
        TerrainMarcher(RHIDevice&, u32 triangleCapacity);
        // the RAII members hold types this header only forward-declares
        ~TerrainMarcher();

        u32 TriangleCapacity() const noexcept{ return triangleCapacity; }
        u32 VertexCapacity() const noexcept{ return triangleCapacity * 3; }

        RHIBuffer& Vertices() noexcept{ return *vertexBuffer; }
        RHIBuffer& Counter() noexcept{ return *counterBuffer; }
        RHIBuffer& Args() noexcept{ return *argsBuffer; }

        // Records the density pass and the marching pass, leaving each buffer
        // where `targets` says. Re-recording is the normal case: the acquires
        // wind the buffers back from wherever the previous submission left
        // them, which is the write-after-read this sample exists to exercise.
        TerrainMarchEdges Record(
            RHICommandList&,
            const TerrainParams&,
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
        u32 _pad0 = 0;

        // how much of the vertex buffer is actually a mesh
        u32 DrawableTriangles() const noexcept{
            return triangleCount < firstDropped ? triangleCount : firstDropped;
        }
    };
    static_assert(sizeof(TerrainMarchCounter) == 16);
}
