#include <cstring>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "IntMath.hpp"
#include "MetalBuffer.hpp"
#include "MetalUtil.hpp"
#include "PtrUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    namespace{
        RHIMemoryClass toMemoryClass(RHIMemoryLocation location){
            using enum RHIMemoryLocation;

            switch(location){
            case Device:   return RHIMemoryClass::Device;
            case Upload:   return RHIMemoryClass::Upload;
            case Readback: return RHIMemoryClass::Readback;
            default:
                std::unreachable();
            }
        }
    }

    MetalBuffer::MetalBuffer(
        MetalAllocator& allocator,
        const RHIBufferCreateDesc& desc,
        StrView name
    )
        : size(desc.size)
        , allocator(allocator)
    {
        using enum RHIBufferUsage;

        const auto isUnorderedAccess = hasFlag(desc.usage, UnorderedAccess);
        const auto isCopyDst = hasFlag(desc.usage, CopyDst);

        const auto isCPUWrite = (desc.cpuAccess == RHICpuAccess::Write);
        CROWY_ASSERT(!isCPUWrite || (!isUnorderedAccess && !isCopyDst));
        const auto isCPURead = (desc.cpuAccess == RHICpuAccess::Read);
        CROWY_ASSERT(!isCPURead || (desc.usage == CopyDst));

        CROWY_ASSERT(
            (desc.location == RHIMemoryLocation::Upload) == isCPUWrite,
            "Upload memory is exactly what the CPU writes"
        );
        CROWY_ASSERT(
            (desc.location == RHIMemoryLocation::Readback) == isCPURead,
            "Readback memory is exactly what the CPU reads"
        );

        allocation = allocator.AllocateBuffer(
            nextMul(desc.size, 16u),
            toMemoryClass(desc.location),
            name
        );
        buffer = static_cast<MTL::Buffer*>(allocation.resource);

        if(isCPUWrite || isCPURead){
            mapped = buffer->contents();
        }

        if(desc.initialData != nullptr && isCPUWrite){
            Upload(desc.initialData, desc.size);
        }
    }

    MetalBuffer::~MetalBuffer(){
        buffer = nullptr;
        mapped = nullptr;
        allocator.Free(allocation);
    }

    void MetalBuffer::Upload(
        const void* data,
        u32 uploadSize,
        u32 offset
    ){
        CROWY_ASSERT(mapped != nullptr, "buffer is not CPU-writable");
        CROWY_ASSERT(uploadSize + offset <= size);

        std::memcpy(
            ptrAdd(mapped, offset),
            data,
            uploadSize
        );
    }

    void MetalBuffer::Download(
        void* data,
        u32 downloadSize,
        u32 offset
    ){
        CROWY_ASSERT(mapped != nullptr, "buffer is not CPU-readable");
        CROWY_ASSERT(downloadSize + offset <= size);

        std::memcpy(
            data,
            ptrAdd(mapped, offset),
            downloadSize
        );
    }

    u64 MetalBuffer::getResourceID(const RHIBufferViewDesc& view){
        // a Metal resource ID is the address itself, so a sub-range view is
        // just that address moved forward
        return buffer->gpuAddress() + view.offset;
    }
}
