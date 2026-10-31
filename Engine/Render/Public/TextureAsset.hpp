#pragma once

#include <expected>
#include <vector>

#include "AssetRoots.hpp"
#include "AssetStore.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderTexture.hpp"

namespace Crowy
{
    struct TextureAsset;

    using TextureStore = AssetStore<TextureAsset>;
    using TextureAssetHandle = AssetHandle<TextureAsset>;

    // how a slot reads a texture's texels; the asset has no preference
    enum class TextureColorSpace : u8 {
        Srgb,
        Linear,
    };

    // engine:Builtin/ holds White, FlatNormal and Missing; any other ref is a
    // PNG, which gets a full mip chain on the CPU, or a KTX2, which keeps the
    // file's mips
    std::expected<TextureAsset, Str> loadTextureAsset(
        RHIDevice& device,
        const AssetRoots& roots,
        const AssetRef& ref
    );
    void releaseTextureAsset(RHIDevice& device, TextureAsset asset);

    // the view a slot reads: its color space, and one mip under a nearest
    // sampler, which would otherwise walk the chain
    RHITextureViewDesc readViewOf(
        RHIPixelFormat format,
        TextureColorSpace space,
        TextureSampler sampler
    );

    struct TextureAsset {
        RHITextureRAII texture;
    };
}
