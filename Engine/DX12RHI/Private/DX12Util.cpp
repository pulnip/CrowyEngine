#include <comdef.h>
#include "DX12Util.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convert(RHIPixelFormat format){
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
        case D16_UNORM:         return DXGI_FORMAT_D16_UNORM;
        case D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case D32_FLOAT:         return DXGI_FORMAT_D32_FLOAT;
        case D32_FLOAT_S8_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        // Block-compressed formats
        case BC1_UNORM:         return DXGI_FORMAT_BC1_UNORM;
        case BC1_UNORM_SRGB:    return DXGI_FORMAT_BC1_UNORM_SRGB;
        case BC2_UNORM:         return DXGI_FORMAT_BC2_UNORM;
        case BC2_UNORM_SRGB:    return DXGI_FORMAT_BC2_UNORM_SRGB;
        case BC3_UNORM:         return DXGI_FORMAT_BC3_UNORM;
        case BC3_UNORM_SRGB:    return DXGI_FORMAT_BC3_UNORM_SRGB;
        case BC4_UNORM:         return DXGI_FORMAT_BC4_UNORM;
        case BC4_SNORM:         return DXGI_FORMAT_BC4_SNORM;
        case BC5_UNORM:         return DXGI_FORMAT_BC5_UNORM;
        case BC5_SNORM:         return DXGI_FORMAT_BC5_SNORM;
        case BC6H_UF16:         return DXGI_FORMAT_BC6H_UF16;
        case BC6H_SF16:         return DXGI_FORMAT_BC6H_SF16;
        case BC7_UNORM:         return DXGI_FORMAT_BC7_UNORM;
        case BC7_UNORM_SRGB:    return DXGI_FORMAT_BC7_UNORM_SRGB;
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

        // Block-compressed formats
        case DXGI_FORMAT_BC1_UNORM:         return BC1_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:    return BC1_UNORM_SRGB;
        case DXGI_FORMAT_BC2_UNORM:         return BC2_UNORM;
        case DXGI_FORMAT_BC2_UNORM_SRGB:    return BC2_UNORM_SRGB;
        case DXGI_FORMAT_BC3_UNORM:         return BC3_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:    return BC3_UNORM_SRGB;
        case DXGI_FORMAT_BC4_UNORM:         return BC4_UNORM;
        case DXGI_FORMAT_BC4_SNORM:         return BC4_SNORM;
        case DXGI_FORMAT_BC5_UNORM:         return BC5_UNORM;
        case DXGI_FORMAT_BC5_SNORM:         return BC5_SNORM;
        case DXGI_FORMAT_BC6H_UF16:         return BC6H_UF16;
        case DXGI_FORMAT_BC6H_SF16:         return BC6H_SF16;
        case DXGI_FORMAT_BC7_UNORM:         return BC7_UNORM;
        case DXGI_FORMAT_BC7_UNORM_SRGB:    return BC7_UNORM_SRGB;
        default:
            return Unknown;
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

    D3D12_BARRIER_SYNC convert(RHIBarrierSync state){
        using enum RHIBarrierSync;

        switch(state){
        case None:                 return D3D12_BARRIER_SYNC_NONE;
        case All:                  return D3D12_BARRIER_SYNC_ALL;
        case Draw:                 return D3D12_BARRIER_SYNC_DRAW;
        case Vertex:               return D3D12_BARRIER_SYNC_VERTEX_SHADING;
        case Fragment:             return D3D12_BARRIER_SYNC_PIXEL_SHADING;
        case DepthStencil:         return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        case RenderTarget:         return D3D12_BARRIER_SYNC_RENDER_TARGET;
        case Compute:              return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        case Copy:                 return D3D12_BARRIER_SYNC_COPY;
        case Resolve:              return D3D12_BARRIER_SYNC_RESOLVE;
        case ExecuteIndirect:      return D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        case AllShading:           return D3D12_BARRIER_SYNC_ALL_SHADING;
        case NonFragment:          return D3D12_BARRIER_SYNC_NON_PIXEL_SHADING;
        case ClearUnorderedAccess: return D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW;
        case Split:                return D3D12_BARRIER_SYNC_SPLIT;
        default:
            std::unreachable();
        }
    }

    D3D12_BARRIER_ACCESS convert(RHIBarrierAccess state){
        using enum RHIBarrierAccess;

        switch(state){
        case Common:            return D3D12_BARRIER_ACCESS_COMMON;
        case VertexBuffer:      return D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
        case ConstantBuffer:    return D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
        case IndexBuffer:       return D3D12_BARRIER_ACCESS_INDEX_BUFFER;
        case RenderTarget:      return D3D12_BARRIER_ACCESS_RENDER_TARGET;
        case UnorderedAccess:   return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        case DepthStencilWrite: return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        case DepthStencilRead:  return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
        case ShaderResource:    return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        case StreamOutput:      return D3D12_BARRIER_ACCESS_STREAM_OUTPUT;
        case IndirectArgument:  return D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
        case CopyDst:           return D3D12_BARRIER_ACCESS_COPY_DEST;
        case CopySrc:           return D3D12_BARRIER_ACCESS_COPY_SOURCE;
        case ResolveDst:        return D3D12_BARRIER_ACCESS_RESOLVE_DEST;
        case ResolveSrc:        return D3D12_BARRIER_ACCESS_RESOLVE_SOURCE;
        case ShadingRateSource: return D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE;
        case NoAccess:          return D3D12_BARRIER_ACCESS_NO_ACCESS;
        default:
            std::unreachable();
        }
    }

    D3D12_BARRIER_LAYOUT convert(RHIBarrierLayout state){
        using enum RHIBarrierLayout;

        switch(state){
        case Undefined:         return D3D12_BARRIER_LAYOUT_UNDEFINED;
        case Common:            return D3D12_BARRIER_LAYOUT_COMMON;
        case GenericRead:       return D3D12_BARRIER_LAYOUT_GENERIC_READ;
        case RenderTarget:      return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        case UnorderedAccess:   return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        case DepthStencilWrite: return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        case DepthStencilRead:  return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        case ShaderResource:    return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
        case CopyDst:           return D3D12_BARRIER_LAYOUT_COPY_DEST;
        case CopySrc:           return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        case ResolveDst:        return D3D12_BARRIER_LAYOUT_RESOLVE_DEST;
        case ResolveSrc:        return D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE;
        case ShadingRateSource: return D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE;
        default:
            std::unreachable();
        }
    }

    Str HResultToString(HRESULT hr){
        _com_error err(hr);
        return static_cast<const char*>(err.ErrorMessage());
    }
}
