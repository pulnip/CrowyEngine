#include <cstdlib>
#include <stdexcept>
#include <d3dx12/d3dx12_core.h>
#include "DX12DescriptorHeapAllocator.hpp"
#include "DX12Util.hpp"

namespace Crowy
{
    DX12DescriptorHeapAllocator::DX12DescriptorHeapAllocator(
        Device& device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT capacity,
        RHIRetireQueue& retireQueue
    )
        : device(device)
        , descriptorSize(device.GetDescriptorHandleIncrementSize(type))
        , type(type)
        , retireQueue(retireQueue)
    #if defined(_DEBUG) || !defined(NDEBUG)
        , poisonMode(std::getenv("CROWY_RHI_POISON") != nullptr)
    #endif
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

    UINT DX12DescriptorHeapAllocator::Allocate(
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

    UINT DX12DescriptorHeapAllocator::Allocate(
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

    UINT DX12DescriptorHeapAllocator::Allocate(
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

    UINT DX12DescriptorHeapAllocator::Allocate(
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

    UINT DX12DescriptorHeapAllocator::Allocate(
        const D3D12_CONSTANT_BUFFER_VIEW_DESC& desc
    ){
        auto index = acquireIndex();
        device.CreateConstantBufferView(
            &desc,
            GetCPUHandle(index)
        );

        return index;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::GetCPUHandle(UINT index) const{
        auto handle = heap->GetCPUDescriptorHandleForHeapStart();
        auto offset = index * descriptorSize;
        handle.ptr += offset;

        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DX12DescriptorHeapAllocator::GetGPUHandle(UINT index) const{
        auto handle = heap->GetGPUDescriptorHandleForHeapStart();
        auto offset = index * descriptorSize;
        handle.ptr += offset;

        return handle;
    }

    UINT DX12DescriptorHeapAllocator::acquireIndex(){
        if(freeIndexes.empty())
            throw std::runtime_error("DescriptorHeap full!");

        UINT index = freeIndexes.back();
        freeIndexes.pop_back();
        return index;
    }

    void DX12DescriptorHeapAllocator::Free(UINT index){
        retireQueue.Defer([this, index]{
        #if defined(_DEBUG) || !defined(NDEBUG)
            if(poisonMode){
                poisonSlot(index);
                return;
            }
        #endif

            freeIndexes.push_back(index);
        });
    }

#if defined(_DEBUG) || !defined(NDEBUG)
    void DX12DescriptorHeapAllocator::poisonSlot(UINT index){
        // only CBV/SRV/UAV descriptors are ever read as a bindless index
        // from shader code; a null raw-buffer SRV is a safe, format-agnostic
        // overwrite regardless of which of the three previously lived here
        if(type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
            return;

        const auto nullDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::RawBuffer(1);
        device.CreateShaderResourceView(
            nullptr,
            &nullDesc,
            GetCPUHandle(index)
        );
    }
#endif
}
