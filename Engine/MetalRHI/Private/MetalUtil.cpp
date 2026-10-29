#include "MetalUtil.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    namespace
    {
        struct PixelFormatPair {
            RHIPixelFormat rhi;
            MTL::PixelFormat metal;
        };

        consteval auto makePixelFormatTable() {
            using enum RHIPixelFormat;
            using namespace MTL;

            return std::to_array<PixelFormatPair>({
                {Unknown, PixelFormatInvalid},
                {R8_UNORM, PixelFormatR8Unorm},
                {R8_SNORM, PixelFormatR8Snorm},
                {R8_UINT, PixelFormatR8Uint},
                {R8_SINT, PixelFormatR8Sint},
                {R16_UNORM, PixelFormatR16Unorm},
                {R16_SNORM, PixelFormatR16Snorm},
                {R16_UINT, PixelFormatR16Uint},
                {R16_SINT, PixelFormatR16Sint},
                {R16_FLOAT, PixelFormatR16Float},
                {RG8_UNORM, PixelFormatRG8Unorm},
                {RG8_SNORM, PixelFormatRG8Snorm},
                {RG8_UINT, PixelFormatRG8Uint},
                {RG8_SINT, PixelFormatRG8Sint},
                {R32_UINT, PixelFormatR32Uint},
                {R32_SINT, PixelFormatR32Sint},
                {R32_FLOAT, PixelFormatR32Float},
                {RG16_UNORM, PixelFormatRG16Unorm},
                {RG16_SNORM, PixelFormatRG16Snorm},
                {RG16_UINT, PixelFormatRG16Uint},
                {RG16_SINT, PixelFormatRG16Sint},
                {RG16_FLOAT, PixelFormatRG16Float},
                {RGBA8_UNORM, PixelFormatRGBA8Unorm},
                {RGBA8_UNORM_SRGB, PixelFormatRGBA8Unorm_sRGB},
                {RGBA8_SNORM, PixelFormatRGBA8Snorm},
                {RGBA8_UINT, PixelFormatRGBA8Uint},
                {RGBA8_SINT, PixelFormatRGBA8Sint},
                {BGRA8_UNORM, PixelFormatBGRA8Unorm},
                {BGRA8_UNORM_SRGB, PixelFormatBGRA8Unorm_sRGB},
                {RG32_UINT, PixelFormatRG32Uint},
                {RG32_SINT, PixelFormatRG32Sint},
                {RG32_FLOAT, PixelFormatRG32Float},
                {RGB32_FLOAT, PixelFormatInvalid},
                {RGBA16_UNORM, PixelFormatRGBA16Unorm},
                {RGBA16_SNORM, PixelFormatRGBA16Snorm},
                {RGBA16_UINT, PixelFormatRGBA16Uint},
                {RGBA16_SINT, PixelFormatRGBA16Sint},
                {RGBA16_FLOAT, PixelFormatRGBA16Float},
                {RGBA32_UINT, PixelFormatRGBA32Uint},
                {RGBA32_SINT, PixelFormatRGBA32Sint},
                {RGBA32_FLOAT, PixelFormatRGBA32Float},
                {D16_UNORM, PixelFormatDepth16Unorm},
                {D24_UNORM_S8_UINT, PixelFormatDepth24Unorm_Stencil8},
                {D32_FLOAT, PixelFormatDepth32Float},
                {D32_FLOAT_S8_UINT, PixelFormatDepth32Float_Stencil8},
                {BC1_UNORM, PixelFormatBC1_RGBA},
                {BC1_UNORM_SRGB, PixelFormatBC1_RGBA_sRGB},
                {BC2_UNORM, PixelFormatBC2_RGBA},
                {BC2_UNORM_SRGB, PixelFormatBC2_RGBA_sRGB},
                {BC3_UNORM, PixelFormatBC3_RGBA},
                {BC3_UNORM_SRGB, PixelFormatBC3_RGBA_sRGB},
                {BC4_UNORM, PixelFormatBC4_RUnorm},
                {BC4_SNORM, PixelFormatBC4_RSnorm},
                {BC5_UNORM, PixelFormatBC5_RGUnorm},
                {BC5_SNORM, PixelFormatBC5_RGSnorm},
                {BC6H_UF16, PixelFormatBC6H_RGBUfloat},
                {BC6H_SF16, PixelFormatBC6H_RGBFloat},
                {BC7_UNORM, PixelFormatBC7_RGBAUnorm},
                {BC7_UNORM_SRGB, PixelFormatBC7_RGBAUnorm_sRGB},
            });
        }

        inline constexpr auto PixelFormats = makePixelFormatTable();

        static_assert(
            PixelFormats.size() ==
            std::to_underlying(RHIPixelFormat::BC7_UNORM_SRGB) + 1
        );
        static_assert(std::ranges::all_of(
            std::views::iota(usize{0}, PixelFormats.size()),
            [](usize i) { return std::to_underlying(PixelFormats[i].rhi) == i; }
        ));
    }

    NS::String* toNSString(StrView str) {
        const Str terminated(str);
        return NS::String::string(terminated.c_str(), NS::UTF8StringEncoding);
    }

    MTL::PixelFormat convert(RHIPixelFormat format) {
        const auto metal = PixelFormats[std::to_underlying(format)].metal;
        CROWY_ASSERT(
            format == RHIPixelFormat::Unknown ||
                metal != MTL::PixelFormatInvalid,
            "no Metal pixel format for RHIPixelFormat {}",
            std::to_underlying(format)
        );

        return metal;
    }

    MTL::CompareFunction convert(RHIComparisonFunc func) {
        using enum RHIComparisonFunc;
        using namespace MTL;

        switch(func) {
        case Never:
            return CompareFunctionNever;
        case Less:
            return CompareFunctionLess;
        case Equal:
            return CompareFunctionEqual;
        case LessEqual:
            return CompareFunctionLessEqual;
        case Greater:
            return CompareFunctionGreater;
        case NotEqual:
            return CompareFunctionNotEqual;
        case GreaterEqual:
            return CompareFunctionGreaterEqual;
        case Always:
            return CompareFunctionAlways;
        default:
            std::unreachable();
        }
    }

    namespace
    {
        auto convert(RHITextureUsage usage) {
            using enum RHITextureUsage;
            MTL::TextureUsage mtlUsage = 0;

            if(hasFlag(usage, ShaderRead))
                mtlUsage |= MTL::TextureUsageShaderRead;
            if(hasFlag(usage, RenderTarget))
                mtlUsage |= MTL::TextureUsageRenderTarget;
            if(hasFlag(usage, DepthStencil))
                mtlUsage |= MTL::TextureUsageRenderTarget;
            if(hasFlag(usage, ShaderWrite))
                mtlUsage |= MTL::TextureUsageShaderWrite;

            return mtlUsage;
        }

        auto convert(u32 depth, u32 arraySize) {
            if(depth != 1) {
                return MTL::TextureType3D;
            }

            return arraySize == 1 ? MTL::TextureType2D
                                  : MTL::TextureType2DArray;
        }
    }

    MTL::TextureDescriptor* convert(const RHITextureCreateDesc& desc) {
        auto texDesc = MTL::TextureDescriptor::alloc()->init();
        texDesc->setWidth(desc.width);
        texDesc->setHeight(desc.height);
        texDesc->setDepth(desc.depth);
        texDesc->setMipmapLevelCount(desc.mipLevels);
        texDesc->setArrayLength(desc.arraySize);
        texDesc->setTextureType(convert(desc.depth, desc.arraySize));
        texDesc->setPixelFormat(convert(desc.format));
        texDesc->setUsage(convert(desc.usage));

        return texDesc;
    }

    RHIPixelFormat convert(MTL::PixelFormat format) {
        const auto it =
            std::ranges::find(PixelFormats, format, &PixelFormatPair::metal);
        CROWY_ASSERT(
            it != PixelFormats.end(),
            "no RHIPixelFormat for MTL::PixelFormat {}",
            std::to_underlying(format)
        );

        return it->rhi;
    }
}
