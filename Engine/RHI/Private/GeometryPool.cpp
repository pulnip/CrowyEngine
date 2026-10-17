#include "GeometryPool.hpp"

#include <cstring>
#include <format>
#include <limits>
#include <optional>

#include <offsetAllocator.hpp>

#include "Assert.hpp"
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
        std::optional<OffsetAllocator::Allocation> allocate(
            OffsetAllocator::Allocator& allocator,
            u32 count
        ) {
            const auto allocation = allocator.allocate(count);
            if(allocation.offset == OffsetAllocator::Allocation::NO_SPACE)
                return std::nullopt;

            return allocation;
        }

        Str outOfSpace(
            const OffsetAllocator::Allocator& allocator,
            CStr what,
            u32 count
        ) {
            return std::format(
                "geometry pool out of {} space ({} elements requested, "
                "largest free run {})",
                what,
                count,
                allocator.storageReport().largestFreeRegion
            );
        }
    }

    GeometryPool::GeometryPool(
        RHIDevice& device,
        u32 vertexCapacity,
        u32 indexCapacity,
        RHIResourceUsage vertexRead,
        u64 stagingBudget
    )
        : device(device)
        , vertexAllocator(std::make_unique<OffsetAllocator::Allocator>(vertexCapacity))
        , indexAllocator(std::make_unique<OffsetAllocator::Allocator>(indexCapacity))
        , vertexCapacity(vertexCapacity)
        , indexCapacity(indexCapacity)
        , vertexRead(vertexRead)
        , stagingBudget(stagingBudget)
    {
        // the staging size is handed to the device as a u32
        CROWY_ASSERT(stagingBudget <= std::numeric_limits<u32>::max());

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

    GeometryPool::AddResult GeometryPool::Add(
        std::span<const Vertex> vertices,
        std::span<const u32> indices
    ) {
        const auto vertexCount = static_cast<u32>(vertices.size());
        const auto indexCount = static_cast<u32>(indices.size());

        const auto staging = geometryStagingBytes(vertexCount, indexCount);
        if(pendingStagingBytes + staging > stagingBudget) {
            return std::unexpected(
                std::format(
                    "geometry pool upload stages {} bytes this frame, past "
                    "its {} budget",
                    pendingStagingBytes + staging,
                    stagingBudget
                )
            );
        }

        const auto vertexAlloc = allocate(*vertexAllocator, vertexCount);
        if(!vertexAlloc)
            return std::unexpected(
                outOfSpace(*vertexAllocator, "vertex", vertexCount)
            );

        const auto indexAlloc = allocate(*indexAllocator, indexCount);
        if(!indexAlloc) {
            vertexAllocator->free(*vertexAlloc);
            return std::unexpected(
                outOfSpace(*indexAllocator, "index", indexCount)
            );
        }

        pendingUploads.push_back(
            PendingUpload{
                .vertices = {vertices.begin(), vertices.end()},
                .indices = {indices.begin(), indices.end()},
                .vertexOffset = vertexAlloc->offset,
                .indexOffset = indexAlloc->offset
            }
        );
        pendingStagingBytes += staging;

        return GeometryAllocation{
            .firstIndex = indexAlloc->offset,
            .indexCount = indexCount,
            .baseVertex = static_cast<i32>(vertexAlloc->offset),
            .vertexMetadata = vertexAlloc->metadata,
            .indexMetadata = indexAlloc->metadata
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

        // staged here rather than at Add: a slice lives until the batch it
        // is recorded into completes, and that batch is only known now
        const auto staging = device.AllocateTransient(
            static_cast<u32>(pendingStagingBytes),
            static_cast<u32>(GeometryStagingAlign)
        );

        RHIEventScope event(cmdList, "GeometryUpload");

        // a later copy waits for the earlier submissions that drew from it
        const auto acquire = [&](RHIBuffer& buffer, RHIResourceUsage read) {
            using enum RHIResourceUsage;
            return everRecorded
                       ? MakeCrossSubmissionBarrier(buffer, read, CopyDst)
                       : MakeBarrier(buffer, Undefined, CopyDst);
        };
        const std::array acquires{
            acquire(*vertexBuffer, vertexRead),
            acquire(*indexBuffer, RHIResourceUsage::IndexBuffer)
        };
        cmdList.BeginBlitPass({}, acquires);

        u64 cursor = 0;
        const auto stage = [&](RHIBuffer& dst, const void* data, u64 bytes, u64 dstOffset){
            std::memcpy(ptrAdd(staging.cpuPtr, cursor), data, bytes);
            cmdList.Copy(*staging.buffer, dst, staging.offset + cursor, dstOffset, bytes);
            cursor = nextMul(cursor + bytes, GeometryStagingAlign);
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
            MakeBarrier(
                *vertexBuffer,
                RHIResourceUsage::CopyDst,
                vertexRead
            ),
            MakeBarrier(
                *indexBuffer,
                RHIResourceUsage::CopyDst,
                RHIResourceUsage::IndexBuffer
            )
        };
        cmdList.EndBlitPass({}, uploadReleases);

        pendingUploads.clear();
        pendingStagingBytes = 0;
        everRecorded = true;

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
