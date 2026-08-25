#pragma once

#include <vector>
#include "DX12Definitions.hpp"
#include "RHIRetireQueue.hpp"

namespace Crowy
{
    class DescriptorHeapAllocator{
    private:
        Device& device;
        DescriptorHeapRAII heap;
        const UINT descriptorSize;
        const D3D12_DESCRIPTOR_HEAP_TYPE type;
        RHIRetireQueue& retireQueue;
        std::vector<UINT> freeIndexes;

    #if defined(_DEBUG) || !defined(NDEBUG)
        // isolates a retired index instead of recycling it, and stamps a
        // null view over it - opt-in, since it never gives an index back
        // and a long debug run can exhaust the heap
        bool poisonMode = false;
    #endif

    public:
        DescriptorHeapAllocator(
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

        UINT Allocate(
            const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc
        );

        UINT Allocate(
            const D3D12_SAMPLER_DESC& desc
        );

        // safe to call from a destructor - the index only becomes available
        // again (or, in poison mode, never does) once the retire queue
        // confirms the GPU is done with whatever last read it
        void Free(UINT index);

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

        DescriptorHeap* Get() noexcept{ return heap.Get(); }

    private:
        UINT acquireIndex();

    #if defined(_DEBUG) || !defined(NDEBUG)
        void poisonSlot(UINT index);
    #endif
    };
}
