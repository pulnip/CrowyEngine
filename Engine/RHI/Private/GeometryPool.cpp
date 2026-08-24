#include <format>
#include <stdexcept>
#include <offsetAllocator.hpp>
#include "EnumUtil.hpp"
#include "GeometryPool.hpp"
#include "IntMath.hpp"
#include "LogLocal.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
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
            .size = vertexCapacity * static_cast<u32>(sizeof(Vertex)),
            .usage = combine(
                RHIBufferUsage::VertexBuffer,
                RHIBufferUsage::ShaderResource,
                RHIBufferUsage::CopyDst
            ),
            .access = RHIMemoryAccess::GPUOnly
        }, "geometry pool vertices");
        indexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = indexCapacity * static_cast<u32>(sizeof(u32)),
            .usage = combine(RHIBufferUsage::IndexBuffer, RHIBufferUsage::CopyDst),
            .access = RHIMemoryAccess::GPUOnly
        }, "geometry pool indices");

        // sized for the whole pool at once: no flush hook, so a request the
        // ring cannot free space for asserts instead of blocking
        const auto stagingBytes = nextMul<u64>(
            vertexCapacity * sizeof(Vertex) + indexCapacity * sizeof(u32),
            512
        );
        auto stagingBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = static_cast<u32>(stagingBytes),
            .usage = RHIBufferUsage::CopySrc,
            .access = RHIMemoryAccess::CPUWrite
        }, "geometry pool staging");
        staging = UploadRing(device, std::move(stagingBuffer));
    }

    GeometryPool::~GeometryPool() = default;

    u64 GeometryPool::GetVertexBufferID(){
        if(vertexBufferID == 0){
            vertexBufferID =
                vertexBuffer->GetReadableID(static_cast<u32>(sizeof(Vertex)));
        }

        return vertexBufferID;
    }

    std::array<RHIBufferBarrier, 2> GeometryPool::UploadAcquires(){
        return {
            MakeBarrier(*vertexBuffer, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst),
            MakeBarrier(*indexBuffer, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst)
        };
    }

    std::array<RHIBufferBarrier, 2> GeometryPool::UploadReleases(){
        return {
            MakeBarrier(*vertexBuffer, RHIResourceUsage::CopyDst, RHIResourceUsage::VertexBuffer),
            MakeBarrier(*indexBuffer, RHIResourceUsage::CopyDst, RHIResourceUsage::IndexBuffer)
        };
    }

    GeometryAllocation GeometryPool::Add(
        RHICommandList& cmdList,
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

        const auto vertexBytes = static_cast<u32>(vertices.size_bytes());
        const auto indexBytes = static_cast<u32>(indices.size_bytes());

        const auto alloc = staging.Allocate(vertexBytes + indexBytes, STAGING_ALIGN);
        const auto stagingOffset = static_cast<u32>(alloc.offset);
        alloc.buffer.Upload(vertices.data(), vertexBytes, stagingOffset);
        alloc.buffer.Upload(indices.data(), indexBytes, stagingOffset + vertexBytes);

        cmdList.Copy(
            alloc.buffer,
            *vertexBuffer,
            stagingOffset,
            vertexAlloc.offset * sizeof(Vertex),
            vertexBytes
        );
        cmdList.Copy(
            alloc.buffer,
            *indexBuffer,
            stagingOffset + vertexBytes,
            indexAlloc.offset * sizeof(u32),
            indexBytes
        );

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
