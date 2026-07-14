#include <d3dx12/d3dx12_core.h>
#include "Assert.hpp"
#include "DescriptorHeapAllocator.hpp"
#include "DX12Definitions.hpp"
#include "EnumUtil.hpp"
#include "RHIDefinitions.hpp"
#include "DX12Texture.hpp"
#include "DX12Util.hpp"

namespace Crowy
{
    DX12Texture::DX12Texture(
        Device& device,
        const RHITextureCreateDesc& desc,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        StrView name
    )
        : RHITexture(
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Undefined
        )
        , cbvsrvuavHeap(cbvsrvuavHeap)
        , rtvHeap(rtvHeap)
        , dsvHeap(dsvHeap)
    {
        using enum RHITextureUsage;
        using enum RHIMemoryAccess;

        const auto heapProp = CD3DX12_HEAP_PROPERTIES(
            D3D12_HEAP_TYPE_DEFAULT
        );

        const auto isShaderResource  = hasFlag(desc.usage, ShaderResource);
        const auto isRenderTarget    = hasFlag(desc.usage, RenderTarget);
        const auto isUnorderedAccess = hasFlag(desc.usage, UnorderedAccess);
        const auto isDepthTarget     = hasFlag(desc.usage, DepthStencil);

        CROWY_ASSERT(!IsBlockCompressed(desc.format) || isShaderResource,
            "Block-compressed textures are shader-resource only"
        );

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if(!isShaderResource) flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        if(isRenderTarget)    flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if(isDepthTarget)     flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if(isUnorderedAccess) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        const auto dxFormat = convert(desc.format, isShaderResource, isDepthTarget);
        const auto texDesc = CD3DX12_RESOURCE_DESC1::Tex2D(
            dxFormat,
            desc.width,
            desc.height,
            desc.arraySize,
            desc.mipLevels,
            1, 0,
            flags
        );

        D3D12_CLEAR_VALUE clearValue{
            .Format = dxFormat
        };
        D3D12_CLEAR_VALUE* pClearValue = nullptr;
        if(isRenderTarget){
            clearValue.Color[0] = desc.clearColor.x;
            clearValue.Color[1] = desc.clearColor.y;
            clearValue.Color[2] = desc.clearColor.z;
            clearValue.Color[3] = desc.clearColor.w;
            pClearValue = &clearValue;
        }
        else if(isDepthTarget){
            clearValue.DepthStencil.Depth = desc.clearDepthStencil.depth;
            clearValue.DepthStencil.Stencil = desc.clearDepthStencil.stencil;
            pClearValue = &clearValue;
        }

        CHECK_HRESULT(device.CreateCommittedResource3(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            // undefined - no content
            convert(GetLayoutState()),
            pClearValue,
            // Hardware DRM
            nullptr,
            // Relaxed Format Casting
            0, nullptr,
            IID_PPV_ARGS(&texture)
        ), "Failed to create DX12 texture");

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            texture->SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()),
                name.data()
            );
        }
    #endif
    }

    DX12Texture::DX12Texture(
        Swapchain& swapchain,
        UINT bufferIndex,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        StrView name
    )
        : RHITexture(
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Present
        )
        , cbvsrvuavHeap(cbvsrvuavHeap)
        , rtvHeap(rtvHeap)
        , dsvHeap(dsvHeap)
    {
        CHECK_HRESULT(swapchain.GetBuffer(
            bufferIndex,
            IID_PPV_ARGS(texture.GetAddressOf())
        ), "Failed to Get Buffer from Swapchain");

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            texture->SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()),
                name.data()
            );
        }
    #endif
    }

    DX12Texture::~DX12Texture(){
        for(const auto& [_, idx]: srvs){
            cbvsrvuavHeap.Free(idx);
        }
        for(const auto& [_, idx]: rtvs){
            rtvHeap.Free(idx);
        }
        for(const auto& [_, idx]: uavs){
            cbvsrvuavHeap.Free(idx);
        }
        for(const auto& [_, idx]: dsvs){
            dsvHeap.Free(idx);
        }
    }

    RHIPixelFormat DX12Texture::GetFormat() const noexcept{
        const auto desc = texture->GetDesc();
        return convert(desc.Format);
    }
    u32 DX12Texture::GetWidth() const noexcept{
        const auto desc = texture->GetDesc();
        return desc.Width;
    }
    u32 DX12Texture::GetHeight() const noexcept{
        const auto desc = texture->GetDesc();
        return desc.Height;
    }
    u16 DX12Texture::GetMipLevels() const noexcept{
        const auto desc = texture->GetDesc();
        return desc.MipLevels;
    }

    UINT DX12Texture::GetOrCreateSRV(const RHITextureViewDesc& desc){
        if(auto it = srvs.find(desc); it != srvs.end())
            return it->second;

        const auto dxDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2D(
            convert(desc.format)
        );

        auto idx = cbvsrvuavHeap.Allocate(
            *texture.Get(),
            dxDesc
        );
        auto [it, ret] = srvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    UINT DX12Texture::GetOrCreateRTV(const RHITextureViewDesc& desc){
        if(auto it = rtvs.find(desc); it != rtvs.end())
            return it->second;

        const D3D12_RENDER_TARGET_VIEW_DESC dxDesc{
            .Format = convert(desc.format),
            .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = D3D12_TEX2D_RTV{
                .MipSlice = 0,
                .PlaneSlice = 0
            }
        };

        auto idx = rtvHeap.Allocate(
            *texture.Get(),
            dxDesc
        );
        auto [it, ret] = rtvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    UINT DX12Texture::GetOrCreateUAV(const RHITextureViewDesc& desc){
        if(auto it = uavs.find(desc); it != uavs.end())
            return it->second;

        const auto dxDesc = CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2D(
            convert(desc.format)
        );

        auto idx = cbvsrvuavHeap.Allocate(
            *texture.Get(),
            dxDesc
        );
        auto [it, ret] = uavs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }


    UINT DX12Texture::GetOrCreateDSV(const RHITextureViewDesc& desc){
        if(auto it = dsvs.find(desc); it != dsvs.end())
            return it->second;

        const D3D12_DEPTH_STENCIL_VIEW_DESC dxDesc{
            .Format = convert(desc.format, false, true),
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D = D3D12_TEX2D_DSV{
                .MipSlice = 0
            }
        };

        auto idx = dsvHeap.Allocate(
            *texture.Get(),
            dxDesc
        );
        auto [it, ret] = dsvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }
}
