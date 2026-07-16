#pragma once

#include <unordered_map>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Texture: public RHITexture{
    private:
        TextureRAII texture = nullptr;
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
            Device&,
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
        u16 GetMipLevels() const noexcept RHI_OVERRIDE;

        void* GetNative() noexcept RHI_OVERRIDE{ return Get(); }

        Texture* Get() noexcept{ return texture.Get(); }

        UINT GetOrCreateSRV(const RHITextureViewDesc&);
        UINT GetOrCreateUAV(const RHITextureViewDesc&);

        UINT GetOrCreateRTV(const RHITextureViewDesc&);
        UINT GetOrCreateDSV(const RHITextureViewDesc&);

        UINT GetOrCreateSRV(){
            return GetOrCreateSRV(RHITextureViewDesc{
                .format = GetFormat()
            });
        }
        u64 GetReadableID() RHI_OVERRIDE{
            return GetOrCreateSRV();
        }
        UINT GetOrCreateUAV(){
            return GetOrCreateUAV(RHITextureViewDesc{
                .format = GetFormat()
            });
        }
        u64 GetWritableID() RHI_OVERRIDE{
            return GetOrCreateUAV();
        }

        UINT GetOrCreateRTV(){
            return GetOrCreateRTV(RHITextureViewDesc{
                .format = GetFormat()
            });
        }
        UINT GetOrCreateDSV(){
            return GetOrCreateDSV(RHITextureViewDesc{
                .format = GetFormat()
            });
        }
    };
}
