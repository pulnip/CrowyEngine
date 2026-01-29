#pragma once

#include <cstddef>
#include <memory>
#include <d3d12.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif
#include "D3D12Util.hpp"
#include "DescriptorHeapAllocator.hpp"

namespace Crowy
{
    class D3D12Texture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        ID3D12Resource* texture = nullptr;
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;
        DescriptorHeapAllocator* srvAllocator = nullptr;
        UINT srvIndex = UINT_MAX;
        DescriptorHeapAllocator* rtvAllocator = nullptr;
        UINT rtvIndex = UINT_MAX;
        DescriptorHeapAllocator* dsvAllocator = nullptr;
        UINT dsvIndex = UINT_MAX;

    public:
        D3D12Texture(
            ID3D12Device* device,
            const RHITextureCreateDesc& desc,
            DescriptorHeapAllocator* srvAllocator,
            DescriptorHeapAllocator* rtvAllocator,
            DescriptorHeapAllocator* dsvAllocator
        )
            : width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
            , srvAllocator(srvAllocator)
            , rtvAllocator(rtvAllocator)
            , dsvAllocator(dsvAllocator)
        {
            CROWY_ASSERT(desc.depth == 1);

            auto isShaderResource  = hasFlag(desc.usage, RHITextureUsage::ShaderResource);
            auto isRenderTarget    = hasFlag(desc.usage, RHITextureUsage::RenderTarget);
            auto isDepthTarget     = hasFlag(desc.usage, RHITextureUsage::DepthStencil);
            auto isUnorderedAccess = hasFlag(desc.usage, RHITextureUsage::UnorderedAccess);

            D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE;
            if(!isShaderResource) bindFlags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            if(isRenderTarget   ) bindFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            if(isDepthTarget    ) bindFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            if(isUnorderedAccess) bindFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            bool needsGPUOnly = isShaderResource || isRenderTarget || 
                                isDepthTarget    || isUnorderedAccess;

            D3D12_RESOURCE_DESC texDesc{
                .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                .Alignment = 0,
                .Width = desc.width,
                .Height = desc.height,
                .DepthOrArraySize = desc.depth > 1 ? desc.depth : desc.arraySize,
                .MipLevels = desc.mipLevels,
                .Format = convertTextureFormat(desc.format),
                // No MSAA
                .SampleDesc = {1, 0},
                .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                .Flags = bindFlags
            };

            D3D12_HEAP_PROPERTIES heapProp{
                .Type = D3D12_HEAP_TYPE_DEFAULT,
                .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN
            };

            D3D12_CLEAR_VALUE clearValue{
                .Format = convertTextureFormat(desc.format)
            };
            D3D12_CLEAR_VALUE* pClearValue = nullptr;

            if(hasFlag(desc.usage, RHITextureUsage::RenderTarget)){
                clearValue.Color[0] = desc.clearColor.r;
                clearValue.Color[1] = desc.clearColor.g;
                clearValue.Color[2] = desc.clearColor.b;
                clearValue.Color[3] = desc.clearColor.a;
                pClearValue = &clearValue;
            }
            else if(hasFlag(desc.usage, RHITextureUsage::DepthStencil)){
                clearValue.DepthStencil.Depth = desc.clearDepthStencil.depth;
                clearValue.DepthStencil.Stencil = desc.clearDepthStencil.stencil;
                pClearValue = &clearValue;
            }

            if(FAILED(device->CreateCommittedResource(
                &heapProp,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                convertResourceState(currentState),
                pClearValue,
                IID_PPV_ARGS(&texture)
            ))){
                throw std::runtime_error("Failed to create D3D12 texture");
            }

            if(desc.initialData != nullptr){
                // TODO
            }

            if(isShaderResource){
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
                    .Format = convertTextureFormat(desc.format, true, false),
                    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Texture2D = {
                        .MostDetailedMip = 0,
                        .MipLevels = 1,
                        .PlaneSlice = 0,
                        .ResourceMinLODClamp = 0.0f
                    }
                };
                srvIndex = srvAllocator->allocate(texture, srvDesc);
            }
            if(isRenderTarget){
                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
                    .Format = convertTextureFormat(desc.format),
                    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        .MipSlice = 0,
                        .PlaneSlice = 0
                    }
                };
                rtvIndex = rtvAllocator->allocate(texture, rtvDesc);
            }
            if(isDepthTarget){
                D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
                    .Format = convertTextureFormat(desc.format, false, true),
                    .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
                    .Flags = D3D12_DSV_FLAG_NONE,
                    .Texture2D = {
                        .MipSlice = 0
                    }
                };
                dsvIndex = dsvAllocator->allocate(texture, dsvDesc);
            }
        }

        ~D3D12Texture(){
            if(srvIndex != UINT_MAX && srvAllocator != nullptr){
                srvAllocator->free(srvIndex);
                srvIndex = UINT_MAX;
            }
            if(rtvIndex != UINT_MAX && rtvAllocator != nullptr){
                rtvAllocator->free(rtvIndex);
                rtvIndex = UINT_MAX;
            }
            if(dsvIndex != UINT_MAX && dsvAllocator != nullptr){
                dsvAllocator->free(dsvIndex);
                dsvIndex = UINT_MAX;
            }
            if(texture != nullptr){
                texture->Release();
                texture = nullptr;
            }
        }

        void uploadData(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            // Deprecated
        }

        RHITextureFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }

        ID3D12Resource* get() const{ return texture; }
        UINT getSRVHeapIndex() const{ return srvIndex; }
        UINT getRTVHeapIndex() const{ return rtvIndex; }
        UINT getDSVHeapIndex() const{ return dsvIndex; }
    };
}
