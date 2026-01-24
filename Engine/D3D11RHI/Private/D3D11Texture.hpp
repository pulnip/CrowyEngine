#pragma once

#include <cstddef>
#include <memory>
#include <d3d11.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif

namespace Crowy
{
    class D3D11Texture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        ID3D11Texture2D* texture = nullptr;
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;

    public:
        D3D11Texture(
            ID3D11Device* device,
            const RHITextureCreateDesc& desc
        )
            : width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
        {}
        ~D3D11Texture() = default;

        void uploadData(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            // No-Op
        }

        ID3D11RenderTargetView* getRTV() const{ return rtv; }
        ID3D11ShaderResourceView* getSRV() const{ return srv; }
        ID3D11DepthStencilView* getDSV() const{ return dsv; }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
