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
        // Legacy Resource Barrier
        RHIResourceState currentState = RHIResourceState::Common;
        // for Enhanced Resource Barrier
        D3D12_BARRIER_SYNC syncState = D3D12_BARRIER_SYNC_NONE;
        D3D12_BARRIER_ACCESS accessState = D3D12_BARRIER_ACCESS_NO_ACCESS;
        D3D12_BARRIER_LAYOUT layoutState = D3D12_BARRIER_LAYOUT_UNDEFINED;
        // descriptor heap index
        std::unordered_map<RHITextureViewDesc, UINT> srvs;
        std::unordered_map<RHITextureViewDesc, UINT> rtvs;
        std::unordered_map<RHITextureViewDesc, UINT> uavs;
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
            UINT bufferIndex,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );

        ~DX12Texture();

        RHIPixelFormat GetFormat() const noexcept RHI_OVERRIDE;
        u32 GetWidth() const noexcept RHI_OVERRIDE;
        u32 GetHeight() const noexcept RHI_OVERRIDE;

        void* GetNative() noexcept RHI_OVERRIDE{ return Get(); }

        RHIResourceState GetState() const noexcept RHI_OVERRIDE{
            return currentState;
        }
        void SetState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }

        // Pipeline Stage Scope
        auto TransitionState(D3D12_BARRIER_SYNC newState) noexcept{
            const auto oldState = syncState;
            syncState = newState;
            return oldState;
        }
        // Cache Visibility
        auto TransitionState(D3D12_BARRIER_ACCESS newState) noexcept{
            const auto oldState = accessState;
            accessState = newState;
            return oldState;
        }
        // Physical Layout in Memory (Texture Only)
        auto TransitionState(D3D12_BARRIER_LAYOUT newState) noexcept{
            const auto oldState = layoutState;
            layoutState = newState;
            return oldState;
        }

        Texture* Get() noexcept{ return texture.Get(); }

        UINT GetOrCreateSRV(const RHITextureViewDesc&);
        UINT GetOrCreateRTV(const RHITextureViewDesc&);
        UINT GetOrCreateUAV(const RHITextureViewDesc&);
        UINT GetOrCreateDSV(const RHITextureViewDesc&);

        UINT GetOrCreateSRV(){
            return GetOrCreateSRV(RHITextureViewDesc{
                .format = GetFormat()
            });
        }
        UINT GetOrCreateRTV(){
            return GetOrCreateRTV(RHITextureViewDesc{
                .format = GetFormat()
            });
        }
        UINT GetOrCreateUAV(){
            return GetOrCreateUAV(RHITextureViewDesc{
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
