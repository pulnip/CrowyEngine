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
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;

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

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
