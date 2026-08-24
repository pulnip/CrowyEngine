#pragma once

#include <unordered_map>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"
#include "DX12Allocator.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Texture: public RHITexture{
    private:
        TextureRAII texture = nullptr;
        // null for a swapchain back buffer: that memory belongs to the
        // swapchain, so nothing here allocated it and nothing frees it
        DX12Allocator* allocator = nullptr;
        RHIAllocation allocation{};
        // descriptor heap index
        std::unordered_map<RHITextureViewDesc, UINT> srvs;
        std::unordered_map<RHITextureViewDesc, UINT> uavs;
        std::unordered_map<RHITextureViewDesc, UINT> rtvs;
        std::unordered_map<RHITextureViewDesc, UINT> dsvs;

        DescriptorHeapAllocator& cbvsrvuavHeap;
        DescriptorHeapAllocator& rtvHeap;
        DescriptorHeapAllocator& dsvHeap;

    public:
        DX12Texture(
            DX12Allocator&,
            const RHITextureCreateDesc&,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );
        DX12Texture(
            Swapchain&,
            RHIPixelFormat logicalFormat,
            UINT bufferIndex,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );

        ~DX12Texture();

        u32 GetWidth() const noexcept RHI_OVERRIDE;
        u32 GetHeight() const noexcept RHI_OVERRIDE;
        u32 GetDepth() const noexcept RHI_OVERRIDE;
        // the overrides above would otherwise hide the per-mip overloads
        using RHITexture::GetWidth;
        using RHITexture::GetHeight;
        using RHITexture::GetDepth;

        void* GetNative() noexcept RHI_OVERRIDE{ return Get(); }

        Texture* Get() noexcept{ return texture.Get(); }

        UINT GetOrCreateRTV(const RHITextureViewDesc&);
        UINT GetOrCreateDSV(const RHITextureViewDesc&);

        u64 GetReadableID(const RHITextureViewDesc&) RHI_OVERRIDE;
        u64 GetWritableID(const RHITextureViewDesc&) RHI_OVERRIDE;

        // render and depth targets always bind exactly one mip
        UINT GetOrCreateRTV(u32 mipLevel){
            return GetOrCreateRTV(RHITextureViewDesc{
                .format = GetFormat(),
                .mostDetailedMip = mipLevel,
                .mipCount = 1
            });
        }
        UINT GetOrCreateDSV(u32 mipLevel){
            return GetOrCreateDSV(RHITextureViewDesc{
                .format = GetFormat(),
                .mostDetailedMip = mipLevel,
                .mipCount = 1
            });
        }
    };
}
