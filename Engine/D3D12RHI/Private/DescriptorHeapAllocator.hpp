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
            UINT capacity,
            bool shaderVisible = true
        )
            : device(device)
        {
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
            for(UINT i=capacity-1; i>=0; --i)
                freeList.push_back(i);
        }

        ~DescriptorHeapAllocator(){
            if(heap != nullptr){
                heap->Release();
                heap = nullptr;
            }
        }

        UINT allocate(D3D12_SAMPLER_DESC& desc){
            auto index = allocateIndex();
            device->CreateSampler(&desc, getCPUHandle(index));

            return index;
        }

        void free(UINT index){
            freeList.push_back(index);
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
    };
}