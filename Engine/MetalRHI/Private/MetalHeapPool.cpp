#include <algorithm>
#include <format>
#include <stdexcept>
#include "MetalHeapPool.hpp"
#include "MetalUtil.hpp"

namespace Crowy
{
    MetalHeapPool::MetalHeapPool(
        MTL::Device& device,
        const MetalHeapPoolDesc& desc,
        StrView name
    )
        : device(&device)
        , desc(desc)
    {
        auto rsDesc = MTL::ResidencySetDescriptor::alloc()->init();
        rsDesc->setInitialCapacity(8);

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            rsDesc->setLabel(toNSString(name));
        }
    #endif

        NS::Error* error = nullptr;

        residencySet = NS::TransferPtr(
            device.newResidencySet(rsDesc, &error)
        );
        const char* str = residencySet->label()->utf8String();

        rsDesc->release();

        if(!residencySet){
            throw std::runtime_error("Failed to create ResidencySet");
        }
    }

    MetalHeapPool::~MetalHeapPool(){
        std::lock_guard lock(mutex);
        device = nullptr;

        heaps.clear();

        residencySet.reset();
    }

    MTL::Buffer* MetalHeapPool::NewBuffer(NS::UInteger length){
        std::lock_guard lock(mutex);

        const MTL::ResourceOptions options = resourceOptions();
        const MTL::SizeAndAlign sizeAlign = device->heapBufferSizeAndAlign(length, options);
        auto heap = acquireHeap(sizeAlign);

        return heap != nullptr ?
            heap->newBuffer(length, options) :
            nullptr;
    }

    MTL::Texture* MetalHeapPool::NewTexture(MTL::TextureDescriptor* texDesc){
        std::lock_guard lock(mutex);

        // forced settings
        texDesc->setStorageMode(desc.storageMode);
        texDesc->setCpuCacheMode(desc.cpuCacheMode);
        texDesc->setHazardTrackingMode(desc.hazardTracking);

        const MTL::SizeAndAlign sizeAlign = device->heapTextureSizeAndAlign(texDesc);
        auto heap = acquireHeap(sizeAlign);

        return heap != nullptr ?
            heap->newTexture(texDesc) :
            nullptr;
    }

    void MetalHeapPool::ReleaseEmptyHeaps(){
        std::lock_guard lock(mutex);

        bool changed = false;
        for(auto it = heaps.begin(); it != heaps.end();){
            auto heap = it->get();
            if(heap->usedSize() == 0){
                if(residencySet){
                    residencySet->removeAllocation(heap);
                    changed = true;
                }
                heap->release();
                it = heaps.erase(it);
            }
            else{
                ++it;
            }
        }

        if (changed)
            residencySet->commit();
    }

    MetalHeapPool::Stats MetalHeapPool::GetStats() const{
        std::lock_guard lock(mutex);

        Stats stats{
            .heapCount = heaps.size(),
            .totalSize = 0,
            .usedSize = 0
        };

        for(auto& heap: heaps){
            stats.totalSize += heap->size();
            stats.usedSize  += heap->usedSize();
        }

        return stats;
    }

    MTL::Heap* MetalHeapPool::addHeapLocked(NS::UInteger minSize){
        const NS::UInteger size = std::max<NS::UInteger>(minSize, desc.heapSize);

        auto heapDesc = MTL::HeapDescriptor::alloc()->init();
        heapDesc->setType(MTL::HeapTypeAutomatic);
        heapDesc->setStorageMode(desc.storageMode);
        heapDesc->setCpuCacheMode(desc.cpuCacheMode);
        heapDesc->setHazardTrackingMode(desc.hazardTracking);
        heapDesc->setSize(size);

        auto heap = device->newHeap(heapDesc);
        heapDesc->release();

        if(heap == nullptr)
            return nullptr;

    #if defined(_DEBUG) || !defined(NDEBUG)
        heap->setLabel(toNSString(
            std::format("heap{} ({}MiB)",
                // residencySet->label()->utf8String(),
                heapCounter++,
                size
            )
        ));
    #endif

        heaps.push_back(NS::TransferPtr(heap));

        if(residencySet){
            residencySet->addAllocation(heap);
            residencySet->commit();
        }

        return heap;
    }

    MTL::Heap* MetalHeapPool::acquireHeap(MTL::SizeAndAlign sizeAlign){
        MTL::Heap* acquired = nullptr;

        for(auto& heap: heaps){
            if(sizeAlign.size <= heap->maxAvailableSize(sizeAlign.align)){
                acquired = heap.get();
                break;
            }
        }

        if(acquired == nullptr){
            // dedicated heap
            acquired = addHeapLocked(sizeAlign.size);
        }

        return acquired;
    }


    MTL::ResourceOptions MetalHeapPool::resourceOptions() const{
        MTL::ResourceOptions options = 0;

        switch (desc.storageMode){
        case MTL::StorageModeShared:
            options |= MTL::ResourceStorageModeShared;
            break;
        case MTL::StorageModeMemoryless:
            options |= MTL::ResourceStorageModeMemoryless;
            break;
        case MTL::StorageModePrivate:
            [[fallthrough]];
        default:
            options |= MTL::ResourceStorageModePrivate;
            break;
        }

        switch (desc.hazardTracking){
        case MTL::HazardTrackingModeUntracked:
            options |= MTL::ResourceHazardTrackingModeUntracked;
            break;
        case MTL::HazardTrackingModeTracked:
            options |= MTL::ResourceHazardTrackingModeTracked;
            break;
        default:
            break;
        }

        if(desc.cpuCacheMode == MTL::CPUCacheModeWriteCombined)
            options |= MTL::ResourceCPUCacheModeWriteCombined;

        return options;
    }
}
