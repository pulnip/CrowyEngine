#include <utility>
#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "MetalUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    NS::String* toNSString(StrView str){
        return NS::String::string(
            str.data(),
            NS::UTF8StringEncoding
        );
    }

    MTL::PixelFormat convert(RHIPixelFormat format){
        using enum RHIPixelFormat;
        using namespace MTL;

        switch(format){
        case Unknown:           return PixelFormatInvalid;
        // 8-bit formats
        case R8_UNORM:          return PixelFormatR8Unorm;
        case R8_SNORM:          return PixelFormatR8Snorm;
        case R8_UINT:           return PixelFormatR8Uint;
        case R8_SINT:           return PixelFormatR8Sint;
        // 16-bit formats
        case R16_UNORM:         return PixelFormatR16Unorm;
        case R16_SNORM:         return PixelFormatR16Snorm;
        case R16_UINT:          return PixelFormatR16Uint;
        case R16_SINT:          return PixelFormatR16Sint;
        case R16_FLOAT:         return PixelFormatR16Float;

        case RG8_UNORM:         return PixelFormatRG8Unorm;
        case RG8_SNORM:         return PixelFormatRG8Snorm;
        case RG8_UINT:          return PixelFormatRG8Uint;
        case RG8_SINT:          return PixelFormatRG8Sint;
        // 32-bit formats
        case R32_UINT:          return PixelFormatR32Uint;
        case R32_SINT:          return PixelFormatR32Sint;
        case R32_FLOAT:         return PixelFormatR32Float;

        case RG16_UNORM:        return PixelFormatRG16Unorm;
        case RG16_SNORM:        return PixelFormatRG16Snorm;
        case RG16_UINT:         return PixelFormatRG16Uint;
        case RG16_SINT:         return PixelFormatRG16Sint;
        case RG16_FLOAT:        return PixelFormatRG16Float;

        case RGBA8_UNORM:       return PixelFormatRGBA8Unorm;
        case RGBA8_UNORM_SRGB:  return PixelFormatRGBA8Unorm_sRGB;
        case RGBA8_SNORM:       return PixelFormatRGBA8Snorm;
        case RGBA8_UINT:        return PixelFormatRGBA8Uint;
        case RGBA8_SINT:        return PixelFormatRGBA8Sint;

        case BGRA8_UNORM:       return PixelFormatBGRA8Unorm;
        case BGRA8_UNORM_SRGB:  return PixelFormatBGRA8Unorm_sRGB;

        // 64-bit formats
        case RG32_UINT:         return PixelFormatRG32Uint;
        case RG32_SINT:         return PixelFormatRG32Sint;
        case RG32_FLOAT:        return PixelFormatRG32Float;

        case RGBA16_UNORM:      return PixelFormatRGBA16Unorm;
        case RGBA16_SNORM:      return PixelFormatRGBA16Snorm;
        case RGBA16_UINT:       return PixelFormatRGBA16Uint;
        case RGBA16_SINT:       return PixelFormatRGBA16Sint;
        case RGBA16_FLOAT:      return PixelFormatRGBA16Float;

        // 128-bit formats
        case RGBA32_UINT:       return PixelFormatRGBA32Uint;
        case RGBA32_SINT:       return PixelFormatRGBA32Sint;
        case RGBA32_FLOAT:      return PixelFormatRGBA32Float;

        // Depth/stencil formats
        case D16_UNORM:         return PixelFormatDepth16Unorm;
        case D24_UNORM_S8_UINT: return PixelFormatDepth24Unorm_Stencil8;
        case D32_FLOAT:         return PixelFormatDepth32Float;
        case D32_FLOAT_S8_UINT: return PixelFormatDepth32Float_Stencil8;

        // Block-compressed formats
        case BC1_UNORM:         return PixelFormatBC1_RGBA;
        case BC1_UNORM_SRGB:    return PixelFormatBC1_RGBA_sRGB;
        case BC2_UNORM:         return PixelFormatBC2_RGBA;
        case BC2_UNORM_SRGB:    return PixelFormatBC2_RGBA_sRGB;
        case BC3_UNORM:         return PixelFormatBC3_RGBA;
        case BC3_UNORM_SRGB:    return PixelFormatBC3_RGBA_sRGB;
        case BC4_UNORM:         return PixelFormatBC4_RUnorm;
        case BC4_SNORM:         return PixelFormatBC4_RSnorm;
        case BC5_UNORM:         return PixelFormatBC5_RGUnorm;
        case BC5_SNORM:         return PixelFormatBC5_RGSnorm;
        case BC6H_UF16:         return PixelFormatBC6H_RGBUfloat;
        case BC6H_SF16:         return PixelFormatBC6H_RGBFloat;
        case BC7_UNORM:         return PixelFormatBC7_RGBAUnorm;
        case BC7_UNORM_SRGB:    return PixelFormatBC7_RGBAUnorm_sRGB;
        default:
            std::unreachable();
        }
    }

    MTL::CompareFunction convert(RHIComparisonFunc func){
        using enum RHIComparisonFunc;
        using namespace MTL;

        switch(func){
        case Never:        return CompareFunctionNever;
        case Less:         return CompareFunctionLess;
        case Equal:        return CompareFunctionEqual;
        case LessEqual:    return CompareFunctionLessEqual;
        case Greater:      return CompareFunctionGreater;
        case NotEqual:     return CompareFunctionNotEqual;
        case GreaterEqual: return CompareFunctionGreaterEqual;
        case Always:       return CompareFunctionAlways;
        default:
            std::unreachable();
        }
    }

    namespace{
        auto convert(RHITextureUsage usage){
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

        auto convert(u32 height, u32 depth, u32 arraySize){
            CROWY_ASSERT(1 <= height && 1 <= depth,
                "Invalid Texture Depth: {}",
                depth
            );
            CROWY_ASSERT(0 < arraySize,
                "ArraySize should be positive"
            );

            if(height == 1){
                return arraySize == 1 ?
                    MTL::TextureType1D :
                    MTL::TextureType1DArray;
            }
            else if(depth == 1){
                return arraySize == 1 ?
                    MTL::TextureType2D :
                    MTL::TextureType2DArray;
            }

            CROWY_ASSERT(arraySize == 1);
            return MTL::TextureType3D;
        }
    }

    MTL::TextureDescriptor* convert(const RHITextureCreateDesc& desc){
        auto texDesc = MTL::TextureDescriptor::alloc()->init();
        texDesc->setWidth(desc.width);
        texDesc->setHeight(desc.height);
        texDesc->setDepth(desc.depth);
        texDesc->setMipmapLevelCount(desc.mipLevels);
        texDesc->setArrayLength(desc.arraySize);
        texDesc->setTextureType(convert(
            desc.height,
            desc.depth,
            desc.arraySize
        ));
        texDesc->setPixelFormat(convert(desc.format));
        texDesc->setUsage(convert(desc.usage));

        using enum RHIMemoryAccess;

        CROWY_ASSERT(desc.access == GPUOnly || desc.access == Transient,
            "Use RHIBuffer for CPU-Accessable Resource"
        );

        texDesc->setStorageMode(desc.access == GPUOnly ?
            MTL::StorageModePrivate : MTL::StorageModeMemoryless
        );

        return texDesc;
    }

    RHIPixelFormat convert(MTL::PixelFormat format){
        using enum RHIPixelFormat;
        using namespace MTL;

        switch(format){
        case PixelFormatInvalid:               return Unknown;
        // 8-bit formats
        case PixelFormatR8Unorm:               return R8_UNORM;
        case PixelFormatR8Snorm:               return R8_SNORM;
        case PixelFormatR8Uint:                return R8_UINT;
        case PixelFormatR8Sint:                return R8_SINT;
        // 16-bit formats
        case PixelFormatR16Unorm:              return R16_UNORM;
        case PixelFormatR16Snorm:              return R16_SNORM;
        case PixelFormatR16Uint:               return R16_UINT;
        case PixelFormatR16Sint:               return R16_SINT;
        case PixelFormatR16Float:              return R16_FLOAT;

        case PixelFormatRG8Unorm:              return RG8_UNORM;
        case PixelFormatRG8Snorm:              return RG8_SNORM;
        case PixelFormatRG8Uint:               return RG8_UINT;
        case PixelFormatRG8Sint:               return RG8_SINT;
        // 32-bit formats
        case PixelFormatR32Uint:               return R32_UINT;
        case PixelFormatR32Sint:               return R32_SINT;
        case PixelFormatR32Float:              return R32_FLOAT;

        case PixelFormatRG16Unorm:             return RG16_UNORM;
        case PixelFormatRG16Snorm:             return RG16_SNORM;
        case PixelFormatRG16Uint:              return RG16_UINT;
        case PixelFormatRG16Sint:              return RG16_SINT;
        case PixelFormatRG16Float:             return RG16_FLOAT;

        case PixelFormatRGBA8Unorm:            return RGBA8_UNORM;
        case PixelFormatRGBA8Unorm_sRGB:       return RGBA8_UNORM_SRGB;
        case PixelFormatRGBA8Snorm:            return RGBA8_SNORM;
        case PixelFormatRGBA8Uint:             return RGBA8_UINT;
        case PixelFormatRGBA8Sint:             return RGBA8_SINT;

        case PixelFormatBGRA8Unorm:            return BGRA8_UNORM;
        case PixelFormatBGRA8Unorm_sRGB:       return BGRA8_UNORM_SRGB;

        // 64-bit formats
        case PixelFormatRG32Uint:              return RG32_UINT;
        case PixelFormatRG32Sint:              return RG32_SINT;
        case PixelFormatRG32Float:             return RG32_FLOAT;

        case PixelFormatRGBA16Unorm:           return RGBA16_UNORM;
        case PixelFormatRGBA16Snorm:           return RGBA16_SNORM;
        case PixelFormatRGBA16Uint:            return RGBA16_UINT;
        case PixelFormatRGBA16Sint:            return RGBA16_SINT;
        case PixelFormatRGBA16Float:           return RGBA16_FLOAT;

        // 128-bit formats
        case PixelFormatRGBA32Uint:            return RGBA32_UINT;
        case PixelFormatRGBA32Sint:            return RGBA32_SINT;
        case PixelFormatRGBA32Float:           return RGBA32_FLOAT;

        // Depth/stencil formats
        case PixelFormatDepth16Unorm:          return D16_UNORM;
        case PixelFormatDepth24Unorm_Stencil8: return D24_UNORM_S8_UINT;
        case PixelFormatDepth32Float:          return D32_FLOAT;
        case PixelFormatDepth32Float_Stencil8: return D32_FLOAT_S8_UINT;

        // Block-compressed formats
        case PixelFormatBC1_RGBA:              return BC1_UNORM;
        case PixelFormatBC1_RGBA_sRGB:         return BC1_UNORM_SRGB;
        case PixelFormatBC2_RGBA:              return BC2_UNORM;
        case PixelFormatBC2_RGBA_sRGB:         return BC2_UNORM_SRGB;
        case PixelFormatBC3_RGBA:              return BC3_UNORM;
        case PixelFormatBC3_RGBA_sRGB:         return BC3_UNORM_SRGB;
        case PixelFormatBC4_RUnorm:            return BC4_UNORM;
        case PixelFormatBC4_RSnorm:            return BC4_SNORM;
        case PixelFormatBC5_RGUnorm:           return BC5_UNORM;
        case PixelFormatBC5_RGSnorm:           return BC5_SNORM;
        case PixelFormatBC6H_RGBUfloat:        return BC6H_UF16;
        case PixelFormatBC6H_RGBFloat:         return BC6H_SF16;
        case PixelFormatBC7_RGBAUnorm:         return BC7_UNORM;
        case PixelFormatBC7_RGBAUnorm_sRGB:    return BC7_UNORM_SRGB;
        default:
            std::unreachable();
        }
    }
}
