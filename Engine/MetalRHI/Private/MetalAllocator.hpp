#pragma once

#include <unordered_map>

#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLResource.hpp>
#include <Metal/MTLTexture.hpp>

#include "MetalDefinitions.hpp"
#include "MetalHeapPool.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // One allocation, borrowed. MetalAllocator owns the resource until Free,
    // so this stays a plain value that can be copied around freely.
    struct MetalAllocation {
        MTL::Resource* resource = nullptr;
    };

    // The single place that turns a description into memory. Picking the
    // heap pool is what the memory type means on this backend.
    class MetalAllocator {
    private:
        MetalHeapPool& privateHeap;
        MetalHeapPool& sharedHeap;
        std::unordered_map<MTL::Resource*, RefPtr<MTL::Resource>> live;

    public:
        MetalAllocator(MetalHeapPool& privateHeap, MetalHeapPool& sharedHeap);
        ~MetalAllocator();
        CROWY_DECLARE_PINNED(MetalAllocator)

        [[nodiscard]] MetalAllocation AllocateBuffer(
            u64 length,
            RHIMemoryType,
            StrView name = {}
        );
        [[nodiscard]] MetalAllocation AllocateTexture(
            MTL::TextureDescriptor*,
            RHIMemoryType,
            StrView name = {}
        );

        void Free(const MetalAllocation&);

    private:
        MetalHeapPool& poolFor(RHIMemoryType) const;
        MetalAllocation track(MTL::Resource*, StrView name);
    };
}
