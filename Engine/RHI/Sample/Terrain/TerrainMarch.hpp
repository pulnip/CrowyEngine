#pragma once

#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "Terrain.hpp"

namespace Crowy
{
    // The GPU side of the terrain: density evaluation and marching cubes, both
    // compute. Nothing comes back to the CPU - the triangle count only ever
    // exists in a buffer.
    //
    // Owns its resources and the barrier bookkeeping between submissions, so a
    // sample only has to say what it wants to do with the results next.
    class TerrainMarcher{
    private:
        u32 triangleCapacity;

        RHIComputePipelineStateRAII clearPSO, densityPSO, marchPSO;

        RHITextureRAII densityTexture;
        RHIBufferRAII vertexBuffer;
        RHIBufferRAII counterBuffer;
        RHIBufferRAII triTableBuffer;

        // what the previous submission left each resource in.
        // Undefined until the first Record
        RHIResourceUsage densityResting = RHIResourceUsage::Undefined;
        RHIResourceUsage vertexResting = RHIResourceUsage::Undefined;
        RHIResourceUsage counterResting = RHIResourceUsage::Undefined;

    public:
        // the release halves Record emitted. The consuming pass has to acquire
        // with these same values - that is what pairs the two ends of an edge.
        struct Edges{
            RHIBufferBarrier vertices;
            RHIBufferBarrier counter;
        };

        TerrainMarcher(RHIDevice&, u32 triangleCapacity);
        // the RAII members hold types this header only forward-declares
        ~TerrainMarcher();

        u32 TriangleCapacity() const noexcept{ return triangleCapacity; }
        u32 VertexCapacity() const noexcept{ return triangleCapacity * 3; }

        RHIBuffer& Vertices() noexcept{ return *vertexBuffer; }
        RHIBuffer& Counter() noexcept{ return *counterBuffer; }

        // Records the density pass and the marching pass, leaving the vertex
        // and counter buffers in `verticesAfter` / `counterAfter`.
        // Re-recording is the normal case: the acquires wind the buffers back
        // from wherever the previous submission left them.
        Edges Record(
            RHICommandList&,
            const TerrainParams&,
            RHIResourceUsage verticesAfter,
            RHIResourceUsage counterAfter
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
