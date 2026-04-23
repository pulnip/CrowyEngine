#include <utility>
#include "MetalUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MTL::PixelFormat convertPixelFormat(RHIPixelFormat format){
        using enum RHIPixelFormat;

        switch(format){
        case Unknown:           return MTL::PixelFormatInvalid;
        // 8-bit formats
        case R8_UNORM:          return MTL::PixelFormatR8Unorm;
        case R8_SNORM:          return MTL::PixelFormatR8Snorm;
        case R8_UINT:           return MTL::PixelFormatR8Uint;
        case R8_SINT:           return MTL::PixelFormatR8Sint;
        // 16-bit formats
        case R16_UNORM:         return MTL::PixelFormatR16Unorm;
        case R16_SNORM:         return MTL::PixelFormatR16Snorm;
        case R16_UINT:          return MTL::PixelFormatR16Uint;
        case R16_SINT:          return MTL::PixelFormatR16Sint;
        case R16_FLOAT:         return MTL::PixelFormatR16Float;

        case RG8_UNORM:         return MTL::PixelFormatRG8Unorm;
        case RG8_SNORM:         return MTL::PixelFormatRG8Snorm;
        case RG8_UINT:          return MTL::PixelFormatRG8Uint;
        case RG8_SINT:          return MTL::PixelFormatRG8Sint;
        // 32-bit formats
        case R32_UINT:          return MTL::PixelFormatR32Uint;
        case R32_SINT:          return MTL::PixelFormatR32Sint;
        case R32_FLOAT:         return MTL::PixelFormatR32Float;

        case RG16_UNORM:        return MTL::PixelFormatRG16Unorm;
        case RG16_SNORM:        return MTL::PixelFormatRG16Snorm;
        case RG16_UINT:         return MTL::PixelFormatRG16Uint;
        case RG16_SINT:         return MTL::PixelFormatRG16Sint;
        case RG16_FLOAT:        return MTL::PixelFormatRG16Float;

        case RGBA8_UNORM:       return MTL::PixelFormatRGBA8Unorm;
        case RGBA8_UNORM_SRGB:  return MTL::PixelFormatRGBA8Unorm_sRGB;
        case RGBA8_SNORM:       return MTL::PixelFormatRGBA8Snorm;
        case RGBA8_UINT:        return MTL::PixelFormatRGBA8Uint;
        case RGBA8_SINT:        return MTL::PixelFormatRGBA8Sint;

        case BGRA8_UNORM:       return MTL::PixelFormatBGRA8Unorm;
        case BGRA8_UNORM_SRGB:  return MTL::PixelFormatBGRA8Unorm_sRGB;

        // 64-bit formats
        case RG32_UINT:         return MTL::PixelFormatRG32Uint;
        case RG32_SINT:         return MTL::PixelFormatRG32Sint;
        case RG32_FLOAT:        return MTL::PixelFormatRG32Float;

        case RGBA16_UNORM:      return MTL::PixelFormatRGBA16Unorm;
        case RGBA16_SNORM:      return MTL::PixelFormatRGBA16Snorm;
        case RGBA16_UINT:       return MTL::PixelFormatRGBA16Uint;
        case RGBA16_SINT:       return MTL::PixelFormatRGBA16Sint;
        case RGBA16_FLOAT:      return MTL::PixelFormatRGBA16Float;

        // 128-bit formats
        case RGBA32_UINT:       return MTL::PixelFormatRGBA32Uint;
        case RGBA32_SINT:       return MTL::PixelFormatRGBA32Sint;
        case RGBA32_FLOAT:      return MTL::PixelFormatRGBA32Float;

        // Depth/stencil formats
        case D16_UNORM:         return MTL::PixelFormatDepth16Unorm;
        case D24_UNORM_S8_UINT: return MTL::PixelFormatDepth24Unorm_Stencil8;
        case D32_FLOAT:         return MTL::PixelFormatDepth32Float;
        case D32_FLOAT_S8_UINT: return MTL::PixelFormatDepth32Float_Stencil8;
        default:
            std::unreachable();
        }
    }

    MTL::CompareFunction convert(RHIComparisonFunc func){
        using enum RHIComparisonFunc;

        switch(func){
        case Never:        return MTL::CompareFunctionNever;
        case Less:         return MTL::CompareFunctionLess;
        case Equal:        return MTL::CompareFunctionEqual;
        case LessEqual:    return MTL::CompareFunctionLessEqual;
        case Greater:      return MTL::CompareFunctionGreater;
        case NotEqual:     return MTL::CompareFunctionNotEqual;
        case GreaterEqual: return MTL::CompareFunctionGreaterEqual;
        case Always:       return MTL::CompareFunctionAlways;
        default:
            std::unreachable();
        }
    }
}