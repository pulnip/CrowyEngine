#pragma once

#include <vector>
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DescriptorHeapAllocator{
    private:
        Device& device;
        DescriptorHeapRAII heap;
        const UINT descriptorSize;
        std::vector<UINT> freeIndexes;

    public:
        DescriptorHeapAllocator(
            Device& device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity
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

        void Free(UINT index){
            freeIndexes.push_back(index);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const;

        DescriptorHeap* Get() noexcept{ return heap.Get(); }

    private:
        UINT acquireIndex();
    };
}
