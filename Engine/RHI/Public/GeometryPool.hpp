#pragma once

#include <array>
#include <expected>
#include <memory>
#include <span>
#include <vector>

#include "IntMath.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "Vertex.hpp"

namespace OffsetAllocator
{
    class Allocator;
}

namespace Crowy
{
    // every staged array starts on this; nothing binds a view on the staging
    // side, so it only has to be a safe upper bound
    inline constexpr u64 GeometryStagingAlign = 16;
    // the most one frame's uploads may stage: a slice of the device's transient
    // ring, with room left for the rest of that frame
    inline constexpr u64 GeometryStagingBudget = 24ull << 20;
    static_assert(GeometryStagingBudget < TransientStagingBytes);

    // the OffsetAllocator rounds a request up to its bin but files a free
    // region rounded down, so an exact capacity can still refuse
    inline constexpr u32 geometryPoolCapacityFor(u64 elements) noexcept {
        return static_cast<u32>(elements + elements / 8 + 1024);
    }

    // what one mesh takes of a frame's staging
    inline constexpr u64 geometryStagingBytes(
        usize vertexCount,
        usize indexCount
    ) {
        return nextMul<u64>(
                   vertexCount * sizeof(Vertex),
                   GeometryStagingAlign
               ) +
               nextMul<u64>(indexCount * sizeof(u32), GeometryStagingAlign);
    }

    // where a mesh landed inside the pool; feeds RHIDrawIndexedArgs
    // (or DrawIndexed) directly
    struct GeometryAllocation {
        u32 firstIndex = 0;
        u32 indexCount = 0;
        i32 baseVertex = 0;
        // suballocator tickets; only GeometryPool::Free reads them
        u32 vertexMetadata = 0;
        u32 indexMetadata = 0;
    };

    // one vertex/index buffer pair, suballocated by OffsetAllocator for every
    // pooled mesh, so draws differ only by firstIndex and baseVertex
    class GeometryPool {
    public:
        using AddResult = std::expected<GeometryAllocation, Str>;

    private:
        // a mesh's data, held from Add until RecordUploads copies it in
        struct PendingUpload {
            std::vector<Vertex> vertices;
            std::vector<u32> indices;
            // element offsets into the pool's buffers
            u32 vertexOffset = 0;
            u32 indexOffset = 0;
        };

        RHIDevice& device;
        RHIBufferRAII vertexBuffer, indexBuffer;
        std::unique_ptr<OffsetAllocator::Allocator> vertexAllocator,
            indexAllocator;
        u32 vertexCapacity = 0, indexCapacity = 0;
        // what the draws read the vertex buffer as, between uploads
        RHIResourceUsage vertexRead = RHIResourceUsage::SampledVertex;
        u64 stagingBudget = GeometryStagingBudget;
        u64 vertexBufferID = 0;
        std::vector<PendingUpload> pendingUploads;
        // what pendingUploads will stage
        u64 pendingStagingBytes = 0;
        // frees the device still holds; each reaches back into this pool
        u32 freesInFlight = 0;
        std::array<RHIBufferBarrier, 2> uploadReleases{};
        // from then on, earlier frames' draws read the buffers under a copy
        bool everRecorded = false;

    public:
        // capacities are element counts (Vertex / u32 index); vertex pulling
        // reads the vertices as a buffer, a bound vertex buffer as VertexBuffer
        GeometryPool(
            RHIDevice&,
            u32 vertexCapacity,
            u32 indexCapacity,
            RHIResourceUsage vertexRead = RHIResourceUsage::SampledVertex,
            u64 stagingBudget = GeometryStagingBudget
        );
        ~GeometryPool();
        CROWY_DECLARE_PINNED(GeometryPool)

        // suballocates now and copies the data for RecordUploads; a mesh that
        // fits neither the pool nor this frame's staging budget takes no space
        [[nodiscard]] AddResult Add(
            std::span<const Vertex> vertices,
            std::span<const u32> indices
        );
        // the range comes back once the batch being recorded now is done on
        // the GPU, so draws already recorded this frame still read it whole
        void Free(const GeometryAllocation&);

        // one blit pass for every copy Add queued; returns the releases each
        // pass drawing from the pool that frame acquires, empty when nothing
        // was queued
        std::span<const RHIBufferBarrier> RecordUploads(RHICommandList&);

        RHIBuffer& GetVertexBuffer() { return *vertexBuffer; }
        RHIBuffer& GetIndexBuffer() { return *indexBuffer; }

        // For vertex pulling.
        u64 GetVertexBufferID();

        void LogAllocationStats() const;
    };
}
