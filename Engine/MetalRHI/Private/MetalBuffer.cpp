#include <cstring>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "Assert.hpp"
#include "IntMath.hpp"
#include "MetalBuffer.hpp"
#include "MetalUtil.hpp"
#include "PtrUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MetalBuffer::MetalBuffer(
        MetalAllocator& allocator,
        const RHIBufferCreateDesc& desc,
        StrView name
    )
        : size(desc.size)
        , allocator(allocator)
    {
        using enum RHIMemoryType;

        CROWY_ASSERT(desc.memory != Transient,
            "tile memory holds render targets, not buffers"
        );
        CROWY_ASSERT(desc.memory == GPUOnly || !desc.shaderWrite,
            "a shader cannot write CPU-visible memory"
        );

        allocation = allocator.AllocateBuffer(
            nextMul(desc.size, 16u),
            desc.memory,
            name
        );
        buffer = static_cast<MTL::Buffer*>(allocation.resource);

        if(desc.memory != GPUOnly){
            mapped = buffer->contents();
        }

        if(desc.initialData != nullptr && desc.memory == CPUWrite){
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
