#pragma once

#include <unordered_map>

#include <Metal/MTLTexture.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

#include "MetalAllocator.hpp"
#include "MetalDefinitions.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    class MetalTexture final: public RHITexture {
    private:
        RefPtr<MTL::Texture> texture;
        // null for a drawable-backed texture: that memory belongs to the
        // layer, so nothing here allocated it and nothing frees it
        MetalAllocator* allocator = nullptr;
        MetalAllocation allocation{};
        // views alias the base texture's heap allocation,
        // so the heap's residency set covers them too
        std::unordered_map<RHITextureViewDesc, RefPtr<MTL::Texture>> views;

    public:
        MetalTexture() = default;

        MetalTexture(
            MetalAllocator&,
            MTL::TextureDescriptor*,
            RHIMemoryType,
            StrView name = {}
        );
        MetalTexture(CA::MetalDrawable*);
        ~MetalTexture();

        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(MetalTexture)

        u32 GetWidth() const noexcept override { return texture->width(); }
        u32 GetHeight() const noexcept override { return texture->height(); }
        u32 GetDepth() const noexcept override { return texture->depth(); }
        // the overrides above would otherwise hide the per-mip overloads
        using RHITexture::GetDepth;
        using RHITexture::GetHeight;
        using RHITexture::GetWidth;

        auto Get() { return texture.get(); }

        virtual void* GetNative() noexcept override { return Get(); }

    private:
        u64 readableIDOf(const RHITextureViewDesc& view) override {
            return getResourceID(view);
        }
        u64 writableIDOf(const RHITextureViewDesc& view) override {
            return getResourceID(view);
        }

        u64 getResourceID(const RHITextureViewDesc&);
    };
}
