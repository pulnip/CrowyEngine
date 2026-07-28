#pragma once

#include <Metal/MTLTexture.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include "RHIAPI.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    class MetalTexture final: public RHITexture{
    private:
        MTL::Texture* texture = nullptr;

    public:
        MetalTexture() = default;

        MetalTexture(
            MTL::Device&,
            MTL::TextureDescriptor*,
            StrView name = {}
        );
        MetalTexture(
            CA::MetalDrawable*
        );
        ~MetalTexture();

        MetalTexture(MetalTexture&&);
        MetalTexture& operator=(MetalTexture&&);

        u32 GetWidth() const noexcept RHI_OVERRIDE{
            return texture->width();
        }
        u32 GetHeight() const noexcept RHI_OVERRIDE{
            return texture->height();
        }
        // the overrides above would otherwise hide the per-mip overloads
        using RHITexture::GetWidth;
        using RHITexture::GetHeight;


        u64 GetReadableID(const RHITextureViewDesc& view) RHI_OVERRIDE{
            return getResourceID(view);
        }
        u64 GetWritableID(const RHITextureViewDesc& view) RHI_OVERRIDE{
            return getResourceID(view);
        }

        auto Get(){ return texture; }

        virtual void* GetNative() noexcept RHI_OVERRIDE{
            return Get();
        }

        // for prevent CA::MetalDrawable::texture() release
        void SetNull() noexcept{
            texture = nullptr;
        }

    private:
        u64 getResourceID(const RHITextureViewDesc&);
    };
}
