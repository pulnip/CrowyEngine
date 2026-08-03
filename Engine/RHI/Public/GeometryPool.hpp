#pragma once

#include <array>
#include <memory>
#include <span>
#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Vertex.hpp"

namespace OffsetAllocator{
    class Allocator;
}

namespace Crowy
{
    // where a mesh landed inside the pool; feeds RHIDrawIndexedArgs
    // (or DrawIndexed) directly
    struct GeometryAllocation{
        u32 firstIndex = 0;
        u32 indexCount = 0;
        i32 baseVertex = 0;
        // suballocator tickets; only GeometryPool::Free reads them
        u32 vertexMetadata = 0;
        u32 indexMetadata = 0;
    };

    // one big vertex/index buffer pair shared by every pooled mesh, so
    // draws differ only by firstIndex/baseVertex and geometry never
    // rebinds between them. Sub-ranges come from OffsetAllocator.
    class GeometryPool{
    private:
        RHIDevice& device;
        RHIBufferRAII vertexBuffer, indexBuffer;
        std::unique_ptr<OffsetAllocator::Allocator> vertexAllocator, indexAllocator;
        u32 vertexCapacity = 0, indexCapacity = 0;
        // stagings live until the pool dies; fine while meshes are packed
        // once at startup, revisit when uploads become streaming
        std::vector<RHIBufferRAII> stagings;

    public:
        // capacities are element counts (Vertex / u32 index)
        GeometryPool(RHIDevice&, u32 vertexCapacity, u32 indexCapacity);
        ~GeometryPool();

        // barriers for the blit pass that fills the pool:
        //   BeginBlitPass({}, pool.UploadAcquires());
        //   pool.Add(...); ...
        //   EndBlitPass({}, pool.UploadReleases());
        // acquires are self-contained (startup packing writes the pool
        // for the first time); revisit when uploads become streaming
        std::array<RHIBufferBarrier, 2> UploadAcquires();
        std::array<RHIBufferBarrier, 2> UploadReleases();

        // records staging copies; call inside the blit pass above
        GeometryAllocation Add(
            RHICommandList&,
            std::span<const Vertex> vertices,
            std::span<const u32> indices
        );
        void Free(const GeometryAllocation&);

        RHIBuffer& GetVertexBuffer(){ return *vertexBuffer; }
        RHIBuffer& GetIndexBuffer(){ return *indexBuffer; }

        void LogAllocationStats() const;
    };
}
