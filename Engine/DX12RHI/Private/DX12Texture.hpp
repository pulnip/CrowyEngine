#pragma once

#include <unordered_map>

#include "DX12Allocator.hpp"
#include "DX12Definitions.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    class DX12Texture: public RHITexture {
    private:
        Texture* texture = nullptr;
        // holds the reference for a swapchain back buffer; null for an
        // allocated texture, whose ownership lives in DX12Allocator::live
        TextureRAII backBufferOwner = nullptr;
        // null for a swapchain back buffer: that memory belongs to the
        // swapchain, so nothing here allocated it and nothing frees it
        DX12Allocator* allocator = nullptr;
        RHIAllocation allocation{};
        // descriptor heap index
        std::unordered_map<RHITextureViewDesc, UINT> srvs;
        std::unordered_map<RHITextureViewDesc, UINT> uavs;
        std::unordered_map<RHITextureViewDesc, UINT> rtvs;
        std::unordered_map<RHITextureViewDesc, UINT> dsvs;

        DX12DescriptorHeapAllocator& cbvsrvuavHeap;
        DX12DescriptorHeapAllocator& rtvHeap;
        DX12DescriptorHeapAllocator& dsvHeap;

    public:
        DX12Texture(
            DX12Allocator&,
            const RHITextureCreateDesc&,
            DX12DescriptorHeapAllocator& cbvsrvuavHeap,
            DX12DescriptorHeapAllocator& rtvHeap,
            DX12DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );
        DX12Texture(
            Swapchain&,
            RHIPixelFormat logicalFormat,
            UINT bufferIndex,
            DX12DescriptorHeapAllocator& cbvsrvuavHeap,
            DX12DescriptorHeapAllocator& rtvHeap,
            DX12DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );

        ~DX12Texture();

        u32 GetWidth() const noexcept RHI_OVERRIDE;
        u32 GetHeight() const noexcept RHI_OVERRIDE;
        u32 GetDepth() const noexcept RHI_OVERRIDE;
        // the overrides above would otherwise hide the per-mip overloads
        using RHITexture::GetDepth;
        using RHITexture::GetHeight;
        using RHITexture::GetWidth;

        void* GetNative() noexcept RHI_OVERRIDE { return Get(); }

        Texture* Get() noexcept { return texture; }

        UINT GetOrCreateRTV(const RHITextureViewDesc&);
        UINT GetOrCreateDSV(const RHITextureViewDesc&);

        // render and depth targets always bind exactly one mip
        UINT GetOrCreateRTV(u32 mipLevel) {
            return GetOrCreateRTV(
                RHITextureViewDesc{
                    .format = GetFormat(),
                    .mostDetailedMip = mipLevel,
                    .mipCount = 1
                }
            );
        }
        UINT GetOrCreateDSV(u32 mipLevel) {
            return GetOrCreateDSV(
                RHITextureViewDesc{
                    .format = GetFormat(),
                    .mostDetailedMip = mipLevel,
                    .mipCount = 1
                }
            );
        }

    private:
        u64 readableIDOf(const RHITextureViewDesc&) RHI_OVERRIDE;
        u64 writableIDOf(const RHITextureViewDesc&) RHI_OVERRIDE;
    };
}
