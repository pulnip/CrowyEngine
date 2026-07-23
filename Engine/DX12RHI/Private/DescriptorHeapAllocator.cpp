#include <stdexcept>
#include "DescriptorHeapAllocator.hpp"
#include "DX12Util.hpp"

namespace Crowy
{
    DescriptorHeapAllocator::DescriptorHeapAllocator(
        Device& device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT capacity
    )
        : device(device)
        , descriptorSize(device.GetDescriptorHandleIncrementSize(type))
    {
        auto shaderVisible =
            (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ||
            (type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = type,
            .NumDescriptors = capacity,
            .Flags = shaderVisible ?
                D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
                D3D12_DESCRIPTOR_HEAP_FLAG_NONE
        };
        CHECK_HRESULT(device.CreateDescriptorHeap(
            &desc,
            IID_PPV_ARGS(&heap)
        ), "Failed to create DescriptorHeap");

        // reserve id = 0 for invalid
        freeIndexes.reserve(capacity);
        for(UINT i=capacity; i>1; --i)
            freeIndexes.push_back(i-1);
    }

    UINT DescriptorHeapAllocator::Allocate(
        ID3D12Resource& resource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateShaderResourceView(
            &resource,
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    UINT DescriptorHeapAllocator::Allocate(
        ID3D12Resource& resource,
        const D3D12_RENDER_TARGET_VIEW_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateRenderTargetView(
            &resource,
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    UINT DescriptorHeapAllocator::Allocate(
        ID3D12Resource& resource,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateUnorderedAccessView(
            &resource,
            nullptr,
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    UINT DescriptorHeapAllocator::Allocate(
        ID3D12Resource& resource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateDepthStencilView(
            &resource,
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    UINT DescriptorHeapAllocator::Allocate(
        const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateConstantBufferView(
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    UINT DescriptorHeapAllocator::Allocate(
        const D3D12_SAMPLER_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateSampler(
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapAllocator::GetCPUHandle(UINT index) const{
        auto handle = heap->GetCPUDescriptorHandleForHeapStart();
        auto offset = index * descriptorSize;
        handle.ptr += offset;

        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapAllocator::GetGPUHandle(UINT index) const{
        auto handle = heap->GetGPUDescriptorHandleForHeapStart();
        auto offset = index * descriptorSize;
        handle.ptr += offset;

        return handle;
    }

    UINT DescriptorHeapAllocator::acquireIndex(){
        if(freeIndexes.empty())
            throw std::runtime_error("DescriptorHeap full!");

        UINT index = freeIndexes.back();
        freeIndexes.pop_back();
        return index;
    }
}
