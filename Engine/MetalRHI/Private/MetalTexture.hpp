#pragma once

#include <unordered_map>
#include <Metal/MTLTexture.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include "RHIAPI.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    class MetalHeapPool;

    class MetalTexture final: public RHITexture{
    private:
        NS::SharedPtr<MTL::Texture> texture;
        // views alias the base texture's heap allocation,
        // so the heap's residency set covers them too
        std::unordered_map<
            RHITextureViewDesc,
            NS::SharedPtr<MTL::Texture>
        > views;

    public:
        MetalTexture() = default;

        MetalTexture(
            MetalHeapPool&,
            MTL::TextureDescriptor*,
            StrView name = {}
        );
        MetalTexture(
            CA::MetalDrawable*
        );
        ~MetalTexture();

        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(MetalTexture)

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

        auto Get(){ return texture.get(); }

        virtual void* GetNative() noexcept RHI_OVERRIDE{
            return Get();
        }

    private:
        u64 getResourceID(const RHITextureViewDesc&);
    };
}
