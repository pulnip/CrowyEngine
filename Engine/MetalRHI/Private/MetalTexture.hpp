#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include "RHIAPI.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    class MetalTexture final: public RHITexture{
    private:
        MTL::Texture* texture;

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

        u32 GetWidth() const noexcept RHI_OVERRIDE{
            return texture->width();
        }
        u32 GetHeight() const noexcept RHI_OVERRIDE{
            return texture->height();
        }
        // the overrides above would otherwise hide the per-mip overloads
        using RHITexture::GetWidth;
        using RHITexture::GetHeight;

        u64 GetReadableID(const RHITextureViewDesc&) RHI_OVERRIDE;
        u64 GetWritableID(const RHITextureViewDesc&) RHI_OVERRIDE;

        virtual void* GetNative() noexcept RHI_OVERRIDE{
            return texture;
        }

        MTL::Texture* Get(){ return texture; }
    };
}
