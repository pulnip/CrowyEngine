#include <cstring>
#include <format>
#include <stdexcept>
#include <offsetAllocator.hpp>
#include "Assert.hpp"
#include "GeometryPool.hpp"
#include "IntMath.hpp"
#include "LogLocal.hpp"
#include "PtrUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDebugScope.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    namespace{
        // one alignment for every staged copy - nothing here ever binds a
        // view on the staging side, so all that matters is a safe upper bound
        inline constexpr u64 STAGING_ALIGN = 16;

        OffsetAllocator::Allocation allocateOrThrow(
            OffsetAllocator::Allocator& allocator,
            u32 count,
            CStr what
        ){
            const auto allocation = allocator.allocate(count);
            if(allocation.offset == OffsetAllocator::Allocation::NO_SPACE){
                throw std::runtime_error(std::format(
                    "geometry pool out of {} space ({} elements requested)",
                    what, count
                ));
            }
            return allocation;
        }
    }

    GeometryPool::GeometryPool(
        RHIDevice& device,
        u32 vertexCapacity,
        u32 indexCapacity
    )
        : device(device)
        , vertexAllocator(std::make_unique<OffsetAllocator::Allocator>(vertexCapacity))
        , indexAllocator(std::make_unique<OffsetAllocator::Allocator>(indexCapacity))
        , vertexCapacity(vertexCapacity)
        , indexCapacity(indexCapacity)
    {
        vertexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = vertexCapacity * static_cast<u32>(sizeof(Vertex))
        }, "geometry pool vertices");
        indexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = indexCapacity * static_cast<u32>(sizeof(u32))
        }, "geometry pool indices");
    }

    GeometryPool::~GeometryPool() = default;

    u64 GeometryPool::GetVertexBufferID(){
        if(vertexBufferID == 0){
            vertexBufferID =
                vertexBuffer->GetReadableID(static_cast<u32>(sizeof(Vertex)));
        }

        return vertexBufferID;
    }

    GeometryAllocation GeometryPool::Add(
        std::span<const Vertex> vertices,
        std::span<const u32> indices
    ){
        const auto vertexAlloc = allocateOrThrow(
            *vertexAllocator,
            static_cast<u32>(vertices.size()),
            "vertex"
        );
        const auto indexAlloc = allocateOrThrow(
            *indexAllocator,
            static_cast<u32>(indices.size()),
            "index"
        );

        pendingUploads.push_back(PendingUpload{
            .vertices = {vertices.begin(), vertices.end()},
            .indices = {indices.begin(), indices.end()},
            .vertexOffset = vertexAlloc.offset,
            .indexOffset = indexAlloc.offset
        });

        return GeometryAllocation{
            .firstIndex = indexAlloc.offset,
            .indexCount = static_cast<u32>(indices.size()),
            .baseVertex = static_cast<i32>(vertexAlloc.offset),
            .vertexMetadata = vertexAlloc.metadata,
            .indexMetadata = indexAlloc.metadata
        };
    }

    void GeometryPool::Free(const GeometryAllocation& allocation){
        vertexAllocator->free(OffsetAllocator::Allocation{
            .offset = static_cast<u32>(allocation.baseVertex),
            .metadata = allocation.vertexMetadata
        });
        indexAllocator->free(OffsetAllocator::Allocation{
            .offset = allocation.firstIndex,
            .metadata = allocation.indexMetadata
        });
    }

    std::span<const RHIBufferBarrier> GeometryPool::RecordUploads(
        RHICommandList& cmdList
    ){
        if(pendingUploads.empty())
            return {};

        // a later upload lands under draws of earlier frames, so it needs a
        // cross-submission acquire - nothing exercises one yet
        CROWY_ASSERT(!uploaded,
            "geometry pool uploads after the first are not supported yet"
        );

        u64 stagingBytes = 0;
        for(const auto& upload: pendingUploads){
            stagingBytes = nextMul(
                stagingBytes + upload.vertices.size() * sizeof(Vertex),
                STAGING_ALIGN
            );
            stagingBytes = nextMul(
                stagingBytes + upload.indices.size() * sizeof(u32),
                STAGING_ALIGN
            );
        }

        // staged here rather than at Add: a slice lives until the batch it
        // is recorded into completes, and that batch is only known now
        const auto staging = device.AllocateTransient(
            static_cast<u32>(stagingBytes),
            static_cast<u32>(STAGING_ALIGN)
        );

        RHIEventScope event(cmdList, "GeometryUpload");

        const std::array acquires{
            MakeBarrier(*vertexBuffer, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst),
            MakeBarrier(*indexBuffer, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst)
        };
        cmdList.BeginBlitPass({}, acquires);

        u64 cursor = 0;
        const auto stage = [&](RHIBuffer& dst, const void* data, u64 bytes, u64 dstOffset){
            std::memcpy(ptrAdd(staging.cpuPtr, cursor), data, bytes);
            cmdList.Copy(*staging.buffer, dst, staging.offset + cursor, dstOffset, bytes);
            cursor = nextMul(cursor + bytes, STAGING_ALIGN);
        };
        for(const auto& upload: pendingUploads){
            stage(
                *vertexBuffer,
                upload.vertices.data(),
                upload.vertices.size() * sizeof(Vertex),
                upload.vertexOffset * sizeof(Vertex)
            );
            stage(
                *indexBuffer,
                upload.indices.data(),
                upload.indices.size() * sizeof(u32),
                upload.indexOffset * sizeof(u32)
            );
        }

        uploadReleases = {
            MakeBarrier(*vertexBuffer, RHIResourceUsage::CopyDst, RHIResourceUsage::VertexBuffer),
            MakeBarrier(*indexBuffer, RHIResourceUsage::CopyDst, RHIResourceUsage::IndexBuffer)
        };
        cmdList.EndBlitPass({}, uploadReleases);

        pendingUploads.clear();
        uploaded = true;

        return uploadReleases;
    }

    void GeometryPool::LogAllocationStats() const{
        const auto vertexReport = vertexAllocator->storageReport();
        const auto indexReport = indexAllocator->storageReport();

        LOG_INFO(
            "geometry pool: vertices {}/{} used (largest free run {}), "
            "indices {}/{} used (largest free run {})",
            vertexCapacity - vertexReport.totalFreeSpace, vertexCapacity,
            vertexReport.largestFreeRegion,
            indexCapacity - indexReport.totalFreeSpace, indexCapacity,
            indexReport.largestFreeRegion
        );
    }
}
