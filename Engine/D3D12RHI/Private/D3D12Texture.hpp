#pragma once

#include <cstddef>
#include <memory>
#include <d3d12.h>
#include <wrl/client.h>
#include "D3D12Util.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12Texture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        ComPtr<ID3D12Resource> texture;
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        D3D12_RESOURCE_STATES currentState;

    public:
        D3D12Texture(
            ID3D12Device* device,
            const RHITextureCreateDesc& desc
        )
            : width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(convertResourceState(desc.initialState))
        {
            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = (desc.depth > 1) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = desc.height;
            resourceDesc.DepthOrArraySize = (desc.depth > 1) ? desc.depth : desc.arraySize;
            resourceDesc.MipLevels = desc.mipLevels;
            resourceDesc.Format = convertTextureFormat(desc.format);
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            if(hasFlag(desc.usage, RHITextureUsage::RenderTarget)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            }
            if(hasFlag(desc.usage, RHITextureUsage::DepthStencil)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            }
            if(hasFlag(desc.usage, RHITextureUsage::UnorderedAccess)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_CLEAR_VALUE clearValue = {};
            D3D12_CLEAR_VALUE* pClearValue = nullptr;

            if(hasFlag(desc.usage, RHITextureUsage::RenderTarget)){
                clearValue.Format = resourceDesc.Format;
                clearValue.Color[0] = desc.clearColor.r;
                clearValue.Color[1] = desc.clearColor.g;
                clearValue.Color[2] = desc.clearColor.b;
                clearValue.Color[3] = desc.clearColor.a;
                pClearValue = &clearValue;
            }
            else if(hasFlag(desc.usage, RHITextureUsage::DepthStencil)){
                clearValue.Format = resourceDesc.Format;
                clearValue.DepthStencil.Depth = desc.clearDepthStencil.depth;
                clearValue.DepthStencil.Stencil = desc.clearDepthStencil.stencil;
                pClearValue = &clearValue;
            }

            if(FAILED(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                currentState,
                pClearValue,
                IID_PPV_ARGS(&texture)
            ))){
                throw std::runtime_error("Failed to create texture");
            }

            if(desc.initialData){
                uploadData(desc.initialData, 0, 0);
            }
        }

        ~D3D12Texture(){
        }

        void uploadData(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) RHI_OVERRIDE{
            // Note: Texture upload requires upload heap and copy command list
            // This would typically be handled by the device's upload mechanism
            // For now, this is a placeholder
        }

        ID3D12Resource* get() const{ return texture.Get(); }

        D3D12_RESOURCE_STATES& getState(){ return currentState; }
    };
}
