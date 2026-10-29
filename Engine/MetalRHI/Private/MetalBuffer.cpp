#include "MetalBuffer.hpp"

#include <cstring>

#include <Metal/Metal.hpp>
#include <TargetConditionals.h>

#include "IntMath.hpp"
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
        : size(desc.size), allocator(allocator) {
        using enum RHIMemoryType;

        allocation = allocator.AllocateBuffer(
            nextMul(desc.size, 16u),
            desc.memory,
            name
        );
        buffer = static_cast<MTL::Buffer*>(allocation.resource);

        if(desc.memory != GPUOnly) {
            mapped = buffer->contents();
        }

        if(desc.initialData != nullptr && desc.memory == CPUWrite) {
            Upload(desc.initialData, desc.size);
        }
    }

    MetalBuffer::~MetalBuffer() {
        buffer = nullptr;
        mapped = nullptr;
        allocator.Free(allocation);
    }

    u64 MetalBuffer::getResourceID(const RHIBufferViewDesc& view) {
        // a Metal resource ID is the address itself, so a sub-range view is
        // just that address moved forward
        return buffer->gpuAddress() + view.offset;
    }
}
