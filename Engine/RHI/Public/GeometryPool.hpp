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
        // a mesh's data, held from Add until RecordUploads copies it in
        struct PendingUpload{
            std::vector<Vertex> vertices;
            std::vector<u32> indices;
            // element offsets into the pool's buffers
            u32 vertexOffset = 0;
            u32 indexOffset = 0;
        };

        RHIDevice& device;
        RHIBufferRAII vertexBuffer, indexBuffer;
        std::unique_ptr<OffsetAllocator::Allocator> vertexAllocator, indexAllocator;
        u32 vertexCapacity = 0, indexCapacity = 0;
        u64 vertexBufferID = 0;
        std::vector<PendingUpload> pendingUploads;
        std::array<RHIBufferBarrier, 2> uploadReleases{};
        bool uploaded = false;

    public:
        // capacities are element counts (Vertex / u32 index)
        GeometryPool(RHIDevice&, u32 vertexCapacity, u32 indexCapacity);
        ~GeometryPool();

        // suballocates now and keeps a copy of the data; nothing touches the
        // GPU until RecordUploads, so a mesh can be added at any time
        GeometryAllocation Add(
            std::span<const Vertex> vertices,
            std::span<const u32> indices
        );
        void Free(const GeometryAllocation&);

        // records every copy Add queued as one blit pass, outside any other
        // pass, and returns the releases every pass drawing from the pool that
        // frame must acquire; empty when nothing was queued
        std::span<const RHIBufferBarrier> RecordUploads(RHICommandList&);

        RHIBuffer& GetVertexBuffer(){ return *vertexBuffer; }
        RHIBuffer& GetIndexBuffer(){ return *indexBuffer; }

        // For vertex pulling.
        u64 GetVertexBufferID();

        void LogAllocationStats() const;
    };
}
