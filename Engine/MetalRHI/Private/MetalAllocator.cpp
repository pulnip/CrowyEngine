#include "MetalAllocator.hpp"

#include "Assert.hpp"
#include "MetalUtil.hpp"

namespace Crowy
{
    MetalAllocator::MetalAllocator(
        MetalHeapPool& privateHeap,
        MetalHeapPool& sharedHeap
    )
        : privateHeap(privateHeap), sharedHeap(sharedHeap) {}

    MetalAllocator::~MetalAllocator() {
        CROWY_ASSERT(
            live.empty(),
            "{} allocations outlived the allocator",
            live.size()
        );
    }

    MetalHeapPool& MetalAllocator::poolFor(RHIMemoryType memory) const {
        using enum RHIMemoryType;

        // Upload and Readback are both CPU-visible, which on Metal is one
        // storage mode rather than two heap types
        return memory == GPUOnly || memory == Transient ? privateHeap
                                                        : sharedHeap;
    }

    MetalAllocation MetalAllocator::track(
        MTL::Resource* resource,
        StrView name
    ) {
        CROWY_ASSERT(resource != nullptr, "heap pool is out of memory");

#if CROWY_DEBUG_BUILD
        if(!name.empty()) {
            resource->setLabel(toNSString(name));
        }
#endif

        const MetalAllocation allocation{.resource = resource};

        const auto [_, inserted] =
            live.emplace(resource, NS::TransferPtr(resource));
        CROWY_ASSERT(inserted);

        return allocation;
    }

    MetalAllocation MetalAllocator::AllocateBuffer(
        u64 length,
        RHIMemoryType memory,
        StrView name
    ) {
        return track(poolFor(memory).NewBuffer(length), name);
    }

    MetalAllocation MetalAllocator::AllocateTexture(
        MTL::TextureDescriptor* desc,
        RHIMemoryType memory,
        StrView name
    ) {
        return track(poolFor(memory).NewTexture(desc), name);
    }

    void MetalAllocator::Free(const MetalAllocation& allocation) {
        if(allocation.resource == nullptr)
            return;

        const auto erased = live.erase(allocation.resource);
        CROWY_ASSERT(
            erased == 1,
            "freed an allocation this allocator never handed out"
        );
    }
}
