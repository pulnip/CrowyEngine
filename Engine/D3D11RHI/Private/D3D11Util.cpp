#include "D3D11Util.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convertTextureFormat(RHIPixelFormat format, bool isShaderResource, bool isDepthTarget){
        using enum RHIPixelFormat;

        switch(format){
        case Unknown:           return DXGI_FORMAT_UNKNOWN;
        // 8-bit formats
        case R8_UNORM:          return DXGI_FORMAT_R8_UNORM;
        case R8_SNORM:          return DXGI_FORMAT_R8_SNORM;
        case R8_UINT:           return DXGI_FORMAT_R8_UINT;
        case R8_SINT:           return DXGI_FORMAT_R8_SINT;
        // 16-bit formats
        case R16_UNORM:         return DXGI_FORMAT_R16_UNORM;
        case R16_SNORM:         return DXGI_FORMAT_R16_SNORM;
        case R16_UINT:          return DXGI_FORMAT_R16_UINT;
        case R16_SINT:          return DXGI_FORMAT_R16_SINT;
        case R16_FLOAT:         return DXGI_FORMAT_R16_FLOAT;

        case RG8_UNORM:         return DXGI_FORMAT_R8G8_UNORM;
        case RG8_SNORM:         return DXGI_FORMAT_R8G8_SNORM;
        case RG8_UINT:          return DXGI_FORMAT_R8G8_UINT;
        case RG8_SINT:          return DXGI_FORMAT_R8G8_SINT;
        // 32-bit formats
        case R32_UINT:          return DXGI_FORMAT_R32_UINT;
        case R32_SINT:          return DXGI_FORMAT_R32_SINT;
        case R32_FLOAT:         return DXGI_FORMAT_R32_FLOAT;

        case RG16_UNORM:        return DXGI_FORMAT_R16G16_UNORM;
        case RG16_SNORM:        return DXGI_FORMAT_R16G16_SNORM;
        case RG16_UINT:         return DXGI_FORMAT_R16G16_UINT;
        case RG16_SINT:         return DXGI_FORMAT_R16G16_SINT;
        case RG16_FLOAT:        return DXGI_FORMAT_R16G16_FLOAT;

        case RGBA8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RGBA8_UNORM_SRGB:  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case RGBA8_SNORM:       return DXGI_FORMAT_R8G8B8A8_SNORM;
        case RGBA8_UINT:        return DXGI_FORMAT_R8G8B8A8_UINT;
        case RGBA8_SINT:        return DXGI_FORMAT_R8G8B8A8_SINT;

        case BGRA8_UNORM:       return DXGI_FORMAT_B8G8R8A8_UNORM;
        case BGRA8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        // 64-bit formats
        case RG32_UINT:         return DXGI_FORMAT_R32G32_UINT;
        case RG32_SINT:         return DXGI_FORMAT_R32G32_SINT;
        case RG32_FLOAT:        return DXGI_FORMAT_R32G32_FLOAT;

        // 96-bit formats
        case RGB32_FLOAT:       return DXGI_FORMAT_R32G32B32_FLOAT;

        case RGBA16_UNORM:      return DXGI_FORMAT_R16G16B16A16_UNORM;
        case RGBA16_SNORM:      return DXGI_FORMAT_R16G16B16A16_SNORM;
        case RGBA16_UINT:       return DXGI_FORMAT_R16G16B16A16_UINT;
        case RGBA16_SINT:       return DXGI_FORMAT_R16G16B16A16_SINT;
        case RGBA16_FLOAT:      return DXGI_FORMAT_R16G16B16A16_FLOAT;

        // 128-bit formats
        case RGBA32_UINT:       return DXGI_FORMAT_R32G32B32A32_UINT;
        case RGBA32_SINT:       return DXGI_FORMAT_R32G32B32A32_SINT;
        case RGBA32_FLOAT:      return DXGI_FORMAT_R32G32B32A32_FLOAT;

        // Depth/stencil formats
        case D16_UNORM:         return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D16_UNORM) :
            DXGI_FORMAT_R16_UNORM;
        case D24_UNORM_S8_UINT: return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT) :
            DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case D32_FLOAT:         return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT) :
            DXGI_FORMAT_R32_FLOAT;
        case D32_FLOAT_S8_UINT: return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_D32_FLOAT_S8X24_UINT) :
            DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        default:
            std::unreachable();
        }
    }

    D3D11_COMPARISON_FUNC convertCompareFunc(RHIComparisonFunc func){
        using enum RHIComparisonFunc;

        switch(func){
        case Never:        return D3D11_COMPARISON_NEVER;
        case Less:         return D3D11_COMPARISON_LESS;
        case Equal:        return D3D11_COMPARISON_EQUAL;
        case LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case Greater:      return D3D11_COMPARISON_GREATER;
        case NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case Always:       return D3D11_COMPARISON_ALWAYS;
        default:
            std::unreachable();
        }
    }
}