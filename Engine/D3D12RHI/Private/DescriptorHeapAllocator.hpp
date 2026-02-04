#pragma once

#include <optional>
#include <stdexcept>
#include <vector>
#include <d3d12.h>

namespace Crowy
{
    class DescriptorHeapAllocator{
    private:
        ID3D12DescriptorHeap* heap;
        UINT descriptorSize;
        std::vector<UINT> freeList;
        ID3D12Device* device;

    public:
        DescriptorHeapAllocator(
            ID3D12Device* device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity
        )
            : device(device)
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
            device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
            descriptorSize = device->GetDescriptorHandleIncrementSize(type);

            freeList.reserve(capacity);
            for(UINT i=capacity; i>0; --i)
                freeList.push_back(i-1);
        }

        ~DescriptorHeapAllocator(){
            if(heap != nullptr){
                heap->Release();
                heap = nullptr;
            }
        }

        UINT allocate(
            ID3D12Resource* resource,
            D3D12_SHADER_RESOURCE_VIEW_DESC& desc
        ){
            auto index = allocateIndex();
            device->CreateShaderResourceView(resource, &desc, getCPUHandle(index));

            return index;
        }

        UINT allocate(
            ID3D12Resource* resource,
            D3D12_RENDER_TARGET_VIEW_DESC& desc
        ){
            auto index = allocateIndex();
            device->CreateRenderTargetView(resource, &desc, getCPUHandle(index));

            return index;
        }

        UINT allocate(
            ID3D12Resource* resource,
            D3D12_DEPTH_STENCIL_VIEW_DESC& desc
        ){
            auto index = allocateIndex();
            device->CreateDepthStencilView(resource, &desc, getCPUHandle(index));

            return index;
        }

        UINT allocate(
            D3D12_CONSTANT_BUFFER_VIEW_DESC& desc
        ){
            auto index = allocateIndex();
            device->CreateConstantBufferView(&desc, getCPUHandle(index));

            return index;
        }

        UINT allocate(D3D12_SAMPLER_DESC& desc){
            auto index = allocateIndex();
            device->CreateSampler(&desc, getCPUHandle(index));

            return index;
        }

        void free(UINT index){
            freeList.push_back(index);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(UINT index) const{
            auto handle = heap->GetCPUDescriptorHandleForHeapStart();
            auto offset = index * descriptorSize;
            handle.ptr += offset;

            return handle;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(UINT index) const{
            auto handle = heap->GetGPUDescriptorHandleForHeapStart();
            auto offset = index * descriptorSize;
            handle.ptr += offset;

            return handle;
        }

        ID3D12DescriptorHeap* get() const { return heap; }

    private:
        UINT allocateIndex(){
            if(freeList.empty())
                throw std::runtime_error("DescriptorHeap full!");

            UINT index = freeList.back();
            freeList.pop_back();
            return index;
        }
    };
}