#include "Assert.hpp"
#include "MetalAllocator.hpp"
#include "MetalUtil.hpp"

namespace Crowy
{
    MetalAllocator::MetalAllocator(
        MetalHeapPool& privateHeap,
        MetalHeapPool& sharedHeap
    )
        : privateHeap(privateHeap)
        , sharedHeap(sharedHeap){}

    MetalAllocator::~MetalAllocator(){
        CROWY_ASSERT(live.empty(),
            "{} allocations outlived the allocator",
            live.size()
        );
    }

    MetalHeapPool& MetalAllocator::poolFor(RHIMemoryType memory) const{
        using enum RHIMemoryType;

        // Upload and Readback are both CPU-visible, which on Metal is one
        // storage mode rather than two heap types
        return memory == GPUOnly || memory == Transient ?
            privateHeap : sharedHeap;
    }

    RHIAllocation MetalAllocator::track(
        MTL::Resource* resource,
        u64 size,
        StrView name
    ){
        CROWY_ASSERT(resource != nullptr, "heap pool is out of memory");

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            resource->setLabel(toNSString(name));
        }
    #endif

        const RHIAllocation allocation{
            .resource = resource,
            .heapOffset = 0,
            .size = size
        };

        const auto [_, inserted] = live.emplace(
            resource,
            NS::TransferPtr(resource)
        );
        CROWY_ASSERT(inserted);

        return allocation;
    }

    RHIAllocation MetalAllocator::AllocateBuffer(
        u64 length,
        RHIMemoryType memory,
        StrView name
    ){
        return track(
            poolFor(memory).NewBuffer(length),
            length,
            name
        );
    }

    RHIAllocation MetalAllocator::AllocateTexture(
        MTL::TextureDescriptor* desc,
        RHIMemoryType memory,
        StrView name
    ){
        auto& pool = poolFor(memory);
        auto* texture = pool.NewTexture(desc);

        return track(
            texture,
            texture != nullptr ? texture->allocatedSize() : 0,
            name
        );
    }

    void MetalAllocator::Free(const RHIAllocation& allocation){
        if(allocation.resource == nullptr)
            return;

        const auto erased = live.erase(allocation.resource);
        CROWY_ASSERT(erased == 1,
            "freed an allocation this allocator never handed out"
        );
    }
}
