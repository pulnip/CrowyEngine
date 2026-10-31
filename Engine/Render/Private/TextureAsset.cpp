#include "TextureAsset.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ImageLoader.hpp"
#include "MipChain.hpp"
#include "RHIDevice.hpp"
#include "StringUtil.hpp"

namespace
{
    constexpr Crowy::StrView BuiltinFolder = "Builtin/";

    struct BuiltinTexture {
        Crowy::StrView name;
        Crowy::ImageData (*make)();
    };

    struct FormatPair {
        Crowy::RHIPixelFormat linear;
        Crowy::RHIPixelFormat srgb;
    };

    constexpr std::array<FormatPair, 5> FormatPairs{{
        {Crowy::RHIPixelFormat::RGBA8_UNORM,
         Crowy::RHIPixelFormat::RGBA8_UNORM_SRGB},
        {Crowy::RHIPixelFormat::BGRA8_UNORM,
         Crowy::RHIPixelFormat::BGRA8_UNORM_SRGB},
        {Crowy::RHIPixelFormat::BC1_UNORM,
         Crowy::RHIPixelFormat::BC1_UNORM_SRGB},
        {Crowy::RHIPixelFormat::BC3_UNORM,
         Crowy::RHIPixelFormat::BC3_UNORM_SRGB},
        {Crowy::RHIPixelFormat::BC7_UNORM,
         Crowy::RHIPixelFormat::BC7_UNORM_SRGB},
    }};

    Crowy::ImageData rgba8Image(
        Crowy::RHIPixelFormat format,
        Crowy::u32 width,
        Crowy::u32 height,
        std::vector<Crowy::u8> texels
    ) {
        Crowy::ImageData image{
            .blob = std::move(texels),
            .format = format,
            .width = width,
            .height = height
        };
        image.subs.push_back(
            Crowy::RHISubresourceData{
                .data = image.blob.data(),
                .rowPitch = static_cast<Crowy::usize>(width) * 4
            }
        );

        return image;
    }

    constexpr std::array<BuiltinTexture, 3> BuiltinTextures{{
        {"White",
         [] {
             return rgba8Image(
                 Crowy::RHIPixelFormat::RGBA8_UNORM_SRGB,
                 1,
                 1,
                 {255, 255, 255, 255}
             );
         }},
        // a tangent-space normal pointing straight out, as data
        {"FlatNormal",
         [] {
             return rgba8Image(
                 Crowy::RHIPixelFormat::RGBA8_UNORM,
                 1,
                 1,
                 {128, 128, 255, 255}
             );
         }},
        // magenta over black, as the Missing material glows
        {"Missing", [] {
             return rgba8Image(
                 Crowy::RHIPixelFormat::RGBA8_UNORM_SRGB,
                 2,
                 2,
                 {255,
                  0,
                  255,
                  255,
                  0,
                  0,
                  0,
                  255,
                  0,
                  0,
                  0,
                  255,
                  255,
                  0,
                  255,
                  255}
             );
         }},
    }};

    Crowy::ImageData readImage(
        const Crowy::AssetRoots& roots,
        const Crowy::AssetRef& ref
    ) {
        using namespace Crowy;

        StrView path = ref.Path();
        if(ref.Mount() == AssetMount::Engine &&
           consumePrefix(path, BuiltinFolder)) {
            const auto found =
                std::ranges::find(BuiltinTextures, path, &BuiltinTexture::name);
            if(found == BuiltinTextures.end()) {
                throw std::runtime_error(
                    std::format(
                        "no built-in texture '{}'; the built-ins are {}",
                        path,
                        join(BuiltinTextures, ", ", &BuiltinTexture::name)
                    )
                );
            }

            auto image = found->make();
            generateMipChain(image);
            return image;
        }

        const auto file = resolveAssetPath(roots, ref);
        auto image = LoadImage(file);
        if(file.extension() != ".ktx2")
            generateMipChain(image);

        return image;
    }

    Crowy::RHIPixelFormat formatIn(
        Crowy::RHIPixelFormat format,
        Crowy::TextureColorSpace space
    ) {
        using enum Crowy::TextureColorSpace;

        for(const auto& pair: FormatPairs) {
            if(format == pair.linear || format == pair.srgb)
                return space == Srgb ? pair.srgb : pair.linear;
        }

        return format;
    }
}

namespace Crowy
{
    std::expected<TextureAsset, Str> loadTextureAsset(
        RHIDevice& device,
        const AssetRoots& roots,
        const AssetRef& ref
    ) {
        try {
            const auto image = readImage(roots, ref);
            if(image.arraySize != 1) {
                return std::unexpected(
                    std::format(
                        "a texture of {} layers; a material reads one",
                        image.arraySize
                    )
                );
            }

            return TextureAsset{
                .texture = device.CreateTexture(
                    RHITextureCreateDesc{
                        .width = image.width,
                        .height = image.height,
                        .mipLevels = image.mipLevels,
                        .format = image.format,
                        .usage = RHITextureUsage::ShaderResource,
                        .initialData = image.subs
                    },
                    ref.ToString()
                )
            };
        }
        catch(const std::exception& error) {
            return std::unexpected(Str(error.what()));
        }
    }

    void releaseTextureAsset(RHIDevice& device, TextureAsset asset) {
        device.Retire(std::move(asset.texture));
    }

    RHITextureViewDesc readViewOf(
        RHIPixelFormat format,
        TextureColorSpace space,
        TextureSampler sampler
    ) {
        return RHITextureViewDesc{
            .format = formatIn(format, space),
            .mipCount = isNearest(sampler) ? 1u : AllMips
        };
    }
}
