#pragma once

#include <mutex>
#include <vector>
#include <Foundation/NSTypes.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLHeap.hpp>
#include <Metal/MTLResidencySet.hpp>
#include <Metal/MTLResource.hpp>
#include <Metal/MTLTexture.hpp>
#include <Foundation/NSSharedPtr.hpp>
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct MetalHeapPoolDesc{
        MTL::StorageMode storageMode = MTL::StorageModePrivate;
        MTL::CPUCacheMode cpuCacheMode = MTL::CPUCacheModeDefaultCache;
        // bindless + explicit barrier
        MTL::HazardTrackingMode hazardTracking = MTL::HazardTrackingModeUntracked;
        // default 128 MiB
        NS::UInteger heapSize = 128ull << 20;
    };

    // for Persistent Resource
    class MetalHeapPool{
    public:
        struct Stats{
            usize heapCount = 0;
            usize totalSize = 0;
            usize usedSize  = 0;
        };

    private:
        MTL::Device* device = nullptr;
        // Metal 4 explicit residency
        NS::SharedPtr<MTL::ResidencySet> residencySet;

        std::vector<NS::SharedPtr<MTL::Heap>> heaps;
        MetalHeapPoolDesc desc{};

        mutable std::mutex mutex;
        u32 heapCounter  = 0;

    public:
        MetalHeapPool() = default;
        ~MetalHeapPool();
        CROWY_DECLARE_NON_COPYABLE(MetalHeapPool)

        MetalHeapPool(
            MTL::Device&,
            const MetalHeapPoolDesc& desc = {},
            StrView name = {}
        );

        MTL::Buffer* NewBuffer(NS::UInteger length);
        // Notice. override storageMode / cpuCacheMode / hazardTrackingMode
        MTL::Texture* NewTexture(MTL::TextureDescriptor*);

        void ReleaseEmptyHeaps();

        Stats GetStats() const;

        auto ResidencySet(){
            return residencySet.get();
        }

    private:
        MTL::Heap* addHeapLocked(NS::UInteger minSize);
        MTL::Heap* acquireHeap(MTL::SizeAndAlign);
        MTL::ResourceOptions resourceOptions() const;
    };
}
