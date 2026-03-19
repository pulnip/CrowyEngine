#pragma once

#include <cstddef>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif

namespace Crowy
{
    class NullTexture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        NullTexture(const RHITextureCreateDesc& desc)
            : width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
        {}
        ~NullTexture() = default;

        void upload(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{

        }

        RHITextureFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }
        size_t getWidth() const noexcept RHI_OVERRIDE{
            return width;
        }
        size_t getHeight() const noexcept RHI_OVERRIDE{
            return height;
        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
