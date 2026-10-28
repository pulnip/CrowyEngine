#pragma once

#include <vector>

#include "DX12Definitions.hpp"
#include "RHIRetireQueue.hpp"

namespace Crowy
{
    class DX12DescriptorHeapAllocator {
    private:
        static constexpr UINT InvalidIndex = 0;

        Device& device;
        RefPtr<DescriptorHeap> heap;
        const UINT descriptorSize;
        const D3D12_DESCRIPTOR_HEAP_TYPE type;
        RHIRetireQueue& retireQueue;
        std::vector<UINT> freeIndexes;

#if CROWY_DEBUG_BUILD
        // isolates a retired index under a null view; opt-in, since it
        // never gives an index back and a long run can exhaust the heap
        bool poisonMode = false;
#endif

    public:
        DX12DescriptorHeapAllocator(
            Device& device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity,
            RHIRetireQueue& retireQueue
        );

        UINT Allocate(
            ID3D12Resource& resource,
            const D3D12_SHADER_RESOURCE_VIEW_DESC& desc
        );

        UINT Allocate(
            ID3D12Resource& resource,
            const D3D12_RENDER_TARGET_VIEW_DESC& desc
        );

        UINT Allocate(
            ID3D12Resource& resource,
            const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc
        );

        UINT Allocate(
            ID3D12Resource& resource,
            const D3D12_DEPTH_STENCIL_VIEW_DESC& desc
        );

        // safe from a destructor: the index comes back (never, in poison mode)
        // once the retire queue confirms the GPU is done with it
        void Free(UINT index);

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;

        DescriptorHeap* Get() noexcept { return heap.Get(); }

    private:
        UINT acquireIndex();

#if CROWY_DEBUG_BUILD
        void poisonSlot(UINT index);
#endif
    };
}
