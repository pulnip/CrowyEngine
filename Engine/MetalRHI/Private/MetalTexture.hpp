#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"
#include "MetalUtil.hpp"

namespace Crowy
{
    class MetalTexture final: public RHITexture{
    private:
        MTL::Texture* texture;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        MetalTexture(
            MTL::Device& device,
            const RHITextureCreateDesc& desc,
            StrView name = {}
        );
        MetalTexture(
            CA::MetalDrawable*
        );
        ~MetalTexture();

        RHIPixelFormat GetFormat() const noexcept RHI_OVERRIDE{
            return convert(texture->pixelFormat());
        }
        u32 GetWidth() const noexcept RHI_OVERRIDE{
            return texture->width();
        }
        u32 GetHeight() const noexcept RHI_OVERRIDE{
            return texture->height();
        }

        virtual void* GetNative() noexcept RHI_OVERRIDE{
            return texture;
        }

        RHIResourceState GetState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void SetState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }

        MTL::Texture* Get(){ return texture; }
    };
}
