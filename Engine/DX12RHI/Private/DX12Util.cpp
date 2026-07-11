#include <comdef.h>
#include "DX12Util.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convert(RHIPixelFormat format, bool isShaderResource, bool isDepthTarget){
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

    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc func){
        using enum RHIComparisonFunc;

        switch(func){
        case Never:        return D3D12_COMPARISON_FUNC_NEVER;
        case Less:         return D3D12_COMPARISON_FUNC_LESS;
        case Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
        case LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case Greater:      return D3D12_COMPARISON_FUNC_GREATER;
        case NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case Always:       return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            std::unreachable();
        }
    }

    D3D12_RESOURCE_STATES convert(RHIResourceState state){
        using enum RHIResourceState;

        switch(state){
        case Common:                    return D3D12_RESOURCE_STATE_COMMON;
        case VertexAndConstantBuffer:   return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case IndexBuffer:               return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case RenderTarget:              return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case UnorderedAccess:           return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case DepthWrite:                return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case DepthRead:                 return D3D12_RESOURCE_STATE_DEPTH_READ;
        case NonFragmentShaderResource: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case FragmentShaderResource:    return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        case StreamOut:                 return D3D12_RESOURCE_STATE_STREAM_OUT;
        case IndirectArgument:          return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        case CopyDst:                   return D3D12_RESOURCE_STATE_COPY_DEST;
        case CopySrc:                   return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case ResolveDst:                return D3D12_RESOURCE_STATE_RESOLVE_DEST;
        case ResolveSrc:                return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        case ShaderResource:            return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
        case GenericRead:               return D3D12_RESOURCE_STATE_GENERIC_READ;
        default:
            std::unreachable();
        }
    }

    RHIPixelFormat convert(DXGI_FORMAT format){
        using enum RHIPixelFormat;

        switch(format){
        case DXGI_FORMAT_UNKNOWN:              return Unknown;
        // 8-bit formats
        case DXGI_FORMAT_R8_UNORM:             return R8_UNORM;
        case DXGI_FORMAT_R8_SNORM:             return R8_SNORM;
        case DXGI_FORMAT_R8_UINT:              return R8_UINT;
        case DXGI_FORMAT_R8_SINT:              return R8_SINT;
        // 16-bit formats
        case DXGI_FORMAT_R16_UNORM:            return R16_UNORM;
        case DXGI_FORMAT_R16_SNORM:            return R16_SNORM;
        case DXGI_FORMAT_R16_UINT:             return R16_UINT;
        case DXGI_FORMAT_R16_SINT:             return R16_SINT;
        case DXGI_FORMAT_R16_FLOAT:            return R16_FLOAT;

        case DXGI_FORMAT_R8G8_UNORM:           return RG8_UNORM;
        case DXGI_FORMAT_R8G8_SNORM:           return RG8_SNORM;
        case DXGI_FORMAT_R8G8_UINT:            return RG8_UINT;
        case DXGI_FORMAT_R8G8_SINT:            return RG8_SINT;
        // 32-bit formats
        case DXGI_FORMAT_R32_UINT:             return R32_UINT;
        case DXGI_FORMAT_R32_SINT:             return R32_SINT;
        case DXGI_FORMAT_R32_FLOAT:            return R32_FLOAT;

        case DXGI_FORMAT_R16G16_UNORM:         return RG16_UNORM;
        case DXGI_FORMAT_R16G16_SNORM:         return RG16_SNORM;
        case DXGI_FORMAT_R16G16_UINT:          return RG16_UINT;
        case DXGI_FORMAT_R16G16_SINT:          return RG16_SINT;
        case DXGI_FORMAT_R16G16_FLOAT:         return RG16_FLOAT;

        case DXGI_FORMAT_R8G8B8A8_UNORM:       return RGBA8_UNORM;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  return RGBA8_UNORM_SRGB;
        case DXGI_FORMAT_R8G8B8A8_SNORM:       return RGBA8_SNORM;
        case DXGI_FORMAT_R8G8B8A8_UINT:        return RGBA8_UINT;
        case DXGI_FORMAT_R8G8B8A8_SINT:        return RGBA8_SINT;

        case DXGI_FORMAT_B8G8R8A8_UNORM:       return BGRA8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  return BGRA8_UNORM_SRGB;

        // 64-bit formats
        case DXGI_FORMAT_R32G32_UINT:          return RG32_UINT;
        case DXGI_FORMAT_R32G32_SINT:          return RG32_SINT;
        case DXGI_FORMAT_R32G32_FLOAT:         return RG32_FLOAT;

        // 96-bit formats
        case DXGI_FORMAT_R32G32B32_FLOAT:      return RGB32_FLOAT;

        case DXGI_FORMAT_R16G16B16A16_UNORM:   return RGBA16_UNORM;
        case DXGI_FORMAT_R16G16B16A16_SNORM:   return RGBA16_SNORM;
        case DXGI_FORMAT_R16G16B16A16_UINT:    return RGBA16_UINT;
        case DXGI_FORMAT_R16G16B16A16_SINT:    return RGBA16_SINT;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:   return RGBA16_FLOAT;

        // 128-bit formats
        case DXGI_FORMAT_R32G32B32A32_UINT:    return RGBA32_UINT;
        case DXGI_FORMAT_R32G32B32A32_SINT:    return RGBA32_SINT;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:   return RGBA32_FLOAT;

        // Depth/stencil formats
        case DXGI_FORMAT_D16_UNORM:            return D16_UNORM;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:    return D24_UNORM_S8_UINT;
        case DXGI_FORMAT_D32_FLOAT:            return D32_FLOAT;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return D32_FLOAT_S8_UINT;
        default:
            return Unknown;
        }
    }

    Str HResultToString(HRESULT hr){
        _com_error err(hr);
        return static_cast<const char*>(err.ErrorMessage());
    }
}
