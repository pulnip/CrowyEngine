#include <format>
#include <stdexcept>
#include <offsetAllocator.hpp>
#include "EnumUtil.hpp"
#include "GeometryPool.hpp"
#include "LogLocal.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    namespace{
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
            .usage = combine(RHIBufferUsage::VertexBuffer, RHIBufferUsage::CopyDst),
            .access = RHIMemoryAccess::GPUOnly
        }, "geometry pool vertices");
        indexBuffer = device.CreateBuffer(RHIBufferCreateDesc{
            .size = indexCapacity * static_cast<u32>(sizeof(u32)),
            .usage = combine(RHIBufferUsage::IndexBuffer, RHIBufferUsage::CopyDst),
            .access = RHIMemoryAccess::GPUOnly
        }, "geometry pool indices");
    }

    GeometryPool::~GeometryPool() = default;

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

        auto staging = device.CreateBuffer(RHIBufferCreateDesc{
            .size = vertexBytes + indexBytes,
            .usage = RHIBufferUsage::CopySrc,
            .access = RHIMemoryAccess::CPUWrite
        }, "geometry pool staging");
        staging->Upload(vertices.data(), vertexBytes);
        staging->Upload(indices.data(), indexBytes, vertexBytes);

        if(!uploading){
            cmdList.TransitionBarrier(*vertexBuffer, RHIResourceUsage::CopyDst);
            cmdList.TransitionBarrier(*indexBuffer, RHIResourceUsage::CopyDst);
            uploading = true;
        }

        cmdList.Copy(
            *staging,
            *vertexBuffer,
            0,
            vertexAlloc.offset * sizeof(Vertex),
            vertexBytes
        );
        cmdList.Copy(
            *staging,
            *indexBuffer,
            vertexBytes,
            indexAlloc.offset * sizeof(u32),
            indexBytes
        );
        stagings.push_back(std::move(staging));

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

    void GeometryPool::FinishUploads(RHICommandList& cmdList){
        cmdList.TransitionBarrier(
            *vertexBuffer,
            RHIResourceUsage::VertexBuffer
        );
        cmdList.TransitionBarrier(
            *indexBuffer,
            RHIResourceUsage::IndexBuffer
        );
        uploading = false;
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
