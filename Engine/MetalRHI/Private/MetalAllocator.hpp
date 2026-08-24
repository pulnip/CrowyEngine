#pragma once

#include <unordered_map>
#include <Foundation/NSSharedPtr.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLResource.hpp>
#include <Metal/MTLTexture.hpp>
#include "MetalHeapPool.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // One allocation, borrowed. MetalAllocator owns the resource until Free,
    // so this stays a plain value that can be copied around freely.
    //
    // `heapOffset` is always 0 here: MTL::Heap hands back a distinct
    // MTL::Resource rather than an offset into one. The field exists so the
    // shape matches the backend where a heap offset is real.
    struct RHIAllocation {
        MTL::Resource* resource = nullptr;
        u64 heapOffset = 0;
        u64 size = 0;
    };

    // The single place that turns a description into memory. Picking the
    // heap pool is what the memory type means on this backend.
    class MetalAllocator {
    private:
        MetalHeapPool& privateHeap;
        MetalHeapPool& sharedHeap;
        std::unordered_map<MTL::Resource*, NS::SharedPtr<MTL::Resource>> live;

    public:
        MetalAllocator(
            MetalHeapPool& privateHeap,
            MetalHeapPool& sharedHeap
        );
        ~MetalAllocator();
        CROWY_DECLARE_PINNED(MetalAllocator)

        [[nodiscard]] RHIAllocation AllocateBuffer(
            u64 length,
            RHIMemoryType,
            StrView name = {}
        );
        [[nodiscard]] RHIAllocation AllocateTexture(
            MTL::TextureDescriptor*,
            RHIMemoryType,
            StrView name = {}
        );

        void Free(const RHIAllocation&);

    private:
        MetalHeapPool& poolFor(RHIMemoryType) const;
        RHIAllocation track(MTL::Resource*, u64 size, StrView name);
    };
}
