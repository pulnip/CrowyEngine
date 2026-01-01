#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <d3d12.h>
#include <wrl/client.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBuffer.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12Buffer
#ifndef USE_STATIC_RHI
        : public RHIBuffer
#endif
    {
    private:
        ComPtr<ID3D12Resource> buffer;
        size_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;
        bool isCPUAccessible = false;
        void* mappedData = nullptr;

    public:
        D3D12Buffer(
            ID3D12Device* device,
            const RHIBufferCreateDesc& desc
        )
            : usage(desc.usage)
            , size(desc.size)
        {
            auto hasVertexUsage = hasFlag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = hasFlag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer);
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = isCPUAccessible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
            heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = desc.size;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            if(hasFlag(desc.usage, RHIBufferUsage::UnorderedAccess)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            D3D12_RESOURCE_STATES initialState = isCPUAccessible
                ? D3D12_RESOURCE_STATE_GENERIC_READ
                : D3D12_RESOURCE_STATE_COMMON;

            if(FAILED(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                initialState,
                nullptr,
                IID_PPV_ARGS(&buffer)
            ))){
                throw std::runtime_error("Failed to create buffer");
            }

            if(desc.initialData){
                update(desc.initialData, desc.size, 0);
            }

            if(isCPUAccessible){
                buffer->Map(0, nullptr, &mappedData);
            }
        }

        ~D3D12Buffer(){
            if(mappedData){
                buffer->Unmap(0, nullptr);
            }
        }

        void update(
            const void* data, size_t updateSize,
            size_t offset
        ) RHI_OVERRIDE{
            if(isCPUAccessible && mappedData){
                memcpy(static_cast<uint8_t*>(mappedData) + offset, data, updateSize);
            }
        }

        ID3D12Resource* get() const{ return buffer.Get(); }
    };
}
