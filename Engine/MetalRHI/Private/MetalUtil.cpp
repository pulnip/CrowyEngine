#include <utility>
#include "MetalUtil.hpp"

namespace Crowy
{
    MTL::PixelFormat convertTextureFormat(RHITextureFormat format){
        switch(format){
        case RHITextureFormat::Unknown:           return MTL::PixelFormatInvalid;
        // 8-bit formats
        case RHITextureFormat::R8_UNORM:          return MTL::PixelFormatR8Unorm;
        case RHITextureFormat::R8_SNORM:          return MTL::PixelFormatR8Snorm;
        case RHITextureFormat::R8_UINT:           return MTL::PixelFormatR8Uint;
        case RHITextureFormat::R8_SINT:           return MTL::PixelFormatR8Sint;
        // 16-bit formats
        case RHITextureFormat::R16_UNORM:         return MTL::PixelFormatR16Unorm;
        case RHITextureFormat::R16_SNORM:         return MTL::PixelFormatR16Snorm;
        case RHITextureFormat::R16_UINT:          return MTL::PixelFormatR16Uint;
        case RHITextureFormat::R16_SINT:          return MTL::PixelFormatR16Sint;
        case RHITextureFormat::R16_FLOAT:         return MTL::PixelFormatR16Float;

        case RHITextureFormat::RG8_UNORM:         return MTL::PixelFormatRG8Unorm;
        case RHITextureFormat::RG8_SNORM:         return MTL::PixelFormatRG8Snorm;
        case RHITextureFormat::RG8_UINT:          return MTL::PixelFormatRG8Uint;
        case RHITextureFormat::RG8_SINT:          return MTL::PixelFormatRG8Sint;
        // 32-bit formats
        case RHITextureFormat::R32_UINT:          return MTL::PixelFormatR32Uint;
        case RHITextureFormat::R32_SINT:          return MTL::PixelFormatR32Sint;
        case RHITextureFormat::R32_FLOAT:         return MTL::PixelFormatR32Float;

        case RHITextureFormat::RG16_UNORM:        return MTL::PixelFormatRG16Unorm;
        case RHITextureFormat::RG16_SNORM:        return MTL::PixelFormatRG16Snorm;
        case RHITextureFormat::RG16_UINT:         return MTL::PixelFormatRG16Uint;
        case RHITextureFormat::RG16_SINT:         return MTL::PixelFormatRG16Sint;
        case RHITextureFormat::RG16_FLOAT:        return MTL::PixelFormatRG16Float;

        case RHITextureFormat::RGBA8_UNORM:       return MTL::PixelFormatRGBA8Unorm;
        case RHITextureFormat::RGBA8_UNORM_SRGB:  return MTL::PixelFormatRGBA8Unorm_sRGB;
        case RHITextureFormat::RGBA8_SNORM:       return MTL::PixelFormatRGBA8Snorm;
        case RHITextureFormat::RGBA8_UINT:        return MTL::PixelFormatRGBA8Uint;
        case RHITextureFormat::RGBA8_SINT:        return MTL::PixelFormatRGBA8Sint;

        case RHITextureFormat::BGRA8_UNORM:       return MTL::PixelFormatBGRA8Unorm;
        case RHITextureFormat::BGRA8_UNORM_SRGB:  return MTL::PixelFormatBGRA8Unorm_sRGB;

        // 64-bit formats
        case RHITextureFormat::RG32_UINT:         return MTL::PixelFormatRG32Uint;
        case RHITextureFormat::RG32_SINT:         return MTL::PixelFormatRG32Sint;
        case RHITextureFormat::RG32_FLOAT:        return MTL::PixelFormatRG32Float;

        case RHITextureFormat::RGBA16_UNORM:      return MTL::PixelFormatRGBA16Unorm;
        case RHITextureFormat::RGBA16_SNORM:      return MTL::PixelFormatRGBA16Snorm;
        case RHITextureFormat::RGBA16_UINT:       return MTL::PixelFormatRGBA16Uint;
        case RHITextureFormat::RGBA16_SINT:       return MTL::PixelFormatRGBA16Sint;
        case RHITextureFormat::RGBA16_FLOAT:      return MTL::PixelFormatRGBA16Float;

        // 128-bit formats
        case RHITextureFormat::RGBA32_UINT:       return MTL::PixelFormatRGBA32Uint;
        case RHITextureFormat::RGBA32_SINT:       return MTL::PixelFormatRGBA32Sint;
        case RHITextureFormat::RGBA32_FLOAT:      return MTL::PixelFormatRGBA32Float;

        // Depth/stencil formats
        case RHITextureFormat::D16_UNORM:         return MTL::PixelFormatDepth16Unorm;
        case RHITextureFormat::D24_UNORM_S8_UINT: return MTL::PixelFormatDepth24Unorm_Stencil8;
        case RHITextureFormat::D32_FLOAT:         return MTL::PixelFormatDepth32Float;
        case RHITextureFormat::D32_FLOAT_S8_UINT: return MTL::PixelFormatDepth32Float_Stencil8;
        default:
            std::unreachable();
        }
    }
}