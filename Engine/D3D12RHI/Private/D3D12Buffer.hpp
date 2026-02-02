#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <d3d12.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "semantics.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBuffer.hpp"
#endif
#include "D3D12Util.hpp"
#include "DescriptorHeapAllocator.hpp"

namespace Crowy
{
    class D3D12Buffer
#ifndef USE_STATIC_RHI
        : public RHIBuffer
#endif
    {
    private:
        ID3D12Resource* buffer = nullptr;
        size_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;
        bool isCPUAccessible = false;
        RHIResourceState currentState = RHIResourceState::Common;
        DescriptorHeapAllocator* allocator = nullptr;
        UINT cbvIndex = UINT_MAX;
        UINT srvIndex = UINT_MAX;

    public:
        D3D12Buffer(
            ID3D12Device* device,
            const RHIBufferCreateDesc& desc,
            DescriptorHeapAllocator* allocator = nullptr
        )
            : usage(desc.usage)
            , size(desc.size)
            , allocator(allocator)
        {
            isCPUAccessible = hasFlag(desc.usage, RHIBufferUsage::CPUWrite);

            D3D12_RESOURCE_DESC bufDesc{
                .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                .Alignment = 0,
                .Width = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer) ?
                    (desc.size + 255) & ~255 : desc.size,
                .Height = 1,
                .DepthOrArraySize = 1,
                .MipLevels = 1,
                .Format = DXGI_FORMAT_UNKNOWN,
                .SampleDesc = {1, 0},
                .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                .Flags = hasFlag(desc.usage, RHIBufferUsage::UnorderedAccess) ?
                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS :
                    D3D12_RESOURCE_FLAG_NONE
            };

            D3D12_HEAP_PROPERTIES heapProp{
                .Type = isCPUAccessible ?
                    D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN
            };

            if(FAILED(device->CreateCommittedResource(
                &heapProp,
                D3D12_HEAP_FLAG_NONE,
                &bufDesc,
                convert(currentState),
                nullptr,
                IID_PPV_ARGS(&buffer)
            ))){
                throw std::runtime_error("Failed to create D3D12 buffer");
            }

            if(desc.initialData != nullptr){
                // TODO
            }

            if(hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer) && allocator != nullptr){
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
                    .BufferLocation = buffer->GetGPUVirtualAddress(),
                    .SizeInBytes = static_cast<UINT>((desc.size + 255) & ~255)
                };

                cbvIndex = allocator->allocate(cbvDesc);
            }
            if(hasFlag(desc.usage, RHIBufferUsage::ShaderResource)){
                CROWY_ASSERT(allocator != nullptr);

                // TODO.
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
                    .Format = DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer = {
                        .FirstElement = 0,
                        .NumElements = 0,
                        .StructureByteStride = 0,
                        .Flags = D3D12_BUFFER_SRV_FLAG_NONE
                    }
                };

                srvIndex = allocator->allocate(buffer, srvDesc);
            }
        }

        ~D3D12Buffer(){
            if(allocator != nullptr){
                if(cbvIndex != UINT_MAX){
                    allocator->free(cbvIndex);
                    cbvIndex = UINT_MAX;
                }
                if(srvIndex != UINT_MAX){
                    allocator->free(srvIndex);
                    srvIndex = UINT_MAX;
                }
                allocator = nullptr;
            }
            if(buffer != nullptr){
                buffer->Release();
                buffer = nullptr;
            }
        }

        CROWY_DECLARE_PINNED(D3D12Buffer)

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }

        auto getGPUAddress  () const{ return buffer->GetGPUVirtualAddress(); }
        auto getCBVHeapIndex() const{ return cbvIndex; }
        auto getSRVHeapIndex() const{ return srvIndex; }
    };
}
