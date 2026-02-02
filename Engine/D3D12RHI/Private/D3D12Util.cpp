#include "D3D12Util.hpp"

namespace Crowy
{
    DXGI_FORMAT convert(RHITextureFormat format, bool isShaderResource, bool isDepthTarget){
        switch(format){
        case RHITextureFormat::Unknown:           return DXGI_FORMAT_UNKNOWN;
        // 8-bit formats
        case RHITextureFormat::R8_UNORM:          return DXGI_FORMAT_R8_UNORM;
        case RHITextureFormat::R8_SNORM:          return DXGI_FORMAT_R8_SNORM;
        case RHITextureFormat::R8_UINT:           return DXGI_FORMAT_R8_UINT;
        case RHITextureFormat::R8_SINT:           return DXGI_FORMAT_R8_SINT;
        // 16-bit formats
        case RHITextureFormat::R16_UNORM:         return DXGI_FORMAT_R16_UNORM;
        case RHITextureFormat::R16_SNORM:         return DXGI_FORMAT_R16_SNORM;
        case RHITextureFormat::R16_UINT:          return DXGI_FORMAT_R16_UINT;
        case RHITextureFormat::R16_SINT:          return DXGI_FORMAT_R16_SINT;
        case RHITextureFormat::R16_FLOAT:         return DXGI_FORMAT_R16_FLOAT;

        case RHITextureFormat::RG8_UNORM:         return DXGI_FORMAT_R8G8_UNORM;
        case RHITextureFormat::RG8_SNORM:         return DXGI_FORMAT_R8G8_SNORM;
        case RHITextureFormat::RG8_UINT:          return DXGI_FORMAT_R8G8_UINT;
        case RHITextureFormat::RG8_SINT:          return DXGI_FORMAT_R8G8_SINT;
        // 32-bit formats
        case RHITextureFormat::R32_UINT:          return DXGI_FORMAT_R32_UINT;
        case RHITextureFormat::R32_SINT:          return DXGI_FORMAT_R32_SINT;
        case RHITextureFormat::R32_FLOAT:         return DXGI_FORMAT_R32_FLOAT;

        case RHITextureFormat::RG16_UNORM:        return DXGI_FORMAT_R16G16_UNORM;
        case RHITextureFormat::RG16_SNORM:        return DXGI_FORMAT_R16G16_SNORM;
        case RHITextureFormat::RG16_UINT:         return DXGI_FORMAT_R16G16_UINT;
        case RHITextureFormat::RG16_SINT:         return DXGI_FORMAT_R16G16_SINT;
        case RHITextureFormat::RG16_FLOAT:        return DXGI_FORMAT_R16G16_FLOAT;

        case RHITextureFormat::RGBA8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RHITextureFormat::RGBA8_UNORM_SRGB:  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case RHITextureFormat::RGBA8_SNORM:       return DXGI_FORMAT_R8G8B8A8_SNORM;
        case RHITextureFormat::RGBA8_UINT:        return DXGI_FORMAT_R8G8B8A8_UINT;
        case RHITextureFormat::RGBA8_SINT:        return DXGI_FORMAT_R8G8B8A8_SINT;

        case RHITextureFormat::BGRA8_UNORM:       return DXGI_FORMAT_B8G8R8A8_UNORM;
        case RHITextureFormat::BGRA8_UNORM_SRGB:  return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        // 64-bit formats
        case RHITextureFormat::RG32_UINT:         return DXGI_FORMAT_R32G32_UINT;
        case RHITextureFormat::RG32_SINT:         return DXGI_FORMAT_R32G32_SINT;
        case RHITextureFormat::RG32_FLOAT:        return DXGI_FORMAT_R32G32_FLOAT;

        // 96-bit formats
        case RHITextureFormat::RGB32_FLOAT:       return DXGI_FORMAT_R32G32B32_FLOAT;

        case RHITextureFormat::RGBA16_UNORM:      return DXGI_FORMAT_R16G16B16A16_UNORM;
        case RHITextureFormat::RGBA16_SNORM:      return DXGI_FORMAT_R16G16B16A16_SNORM;
        case RHITextureFormat::RGBA16_UINT:       return DXGI_FORMAT_R16G16B16A16_UINT;
        case RHITextureFormat::RGBA16_SINT:       return DXGI_FORMAT_R16G16B16A16_SINT;
        case RHITextureFormat::RGBA16_FLOAT:      return DXGI_FORMAT_R16G16B16A16_FLOAT;

        // 128-bit formats
        case RHITextureFormat::RGBA32_UINT:       return DXGI_FORMAT_R32G32B32A32_UINT;
        case RHITextureFormat::RGBA32_SINT:       return DXGI_FORMAT_R32G32B32A32_SINT;
        case RHITextureFormat::RGBA32_FLOAT:      return DXGI_FORMAT_R32G32B32A32_FLOAT;

        // Depth/stencil formats
        case RHITextureFormat::D16_UNORM:         return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_D16_UNORM) :
            DXGI_FORMAT_R16_UNORM;
        case RHITextureFormat::D24_UNORM_S8_UINT: return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT) :
            DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case RHITextureFormat::D32_FLOAT:         return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT) :
            DXGI_FORMAT_R32_FLOAT;
        case RHITextureFormat::D32_FLOAT_S8_UINT: return isDepthTarget ?
            (isShaderResource ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_D32_FLOAT_S8X24_UINT) :
            DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        default:
            std::unreachable();
        }
    }

    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc func){
        switch(func){
        case RHIComparisonFunc::Never:        return D3D12_COMPARISON_FUNC_NEVER;
        case RHIComparisonFunc::Less:         return D3D12_COMPARISON_FUNC_LESS;
        case RHIComparisonFunc::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
        case RHIComparisonFunc::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case RHIComparisonFunc::Greater:      return D3D12_COMPARISON_FUNC_GREATER;
        case RHIComparisonFunc::NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case RHIComparisonFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case RHIComparisonFunc::Always:       return D3D12_COMPARISON_FUNC_ALWAYS;
        default:                              return D3D12_COMPARISON_FUNC_ALWAYS;
        }
    }

    D3D12_RESOURCE_STATES convert(RHIResourceState state){
        switch(state){
        case RHIResourceState::Common:            return D3D12_RESOURCE_STATE_COMMON;
        case RHIResourceState::VertexBuffer:      return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case RHIResourceState::IndexBuffer:       return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case RHIResourceState::ConstantBuffer:    return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case RHIResourceState::ShaderResource:    return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case RHIResourceState::UnorderedAccess:   return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case RHIResourceState::RenderTarget:      return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case RHIResourceState::DepthStencilWrite: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case RHIResourceState::DepthStencilRead:  return D3D12_RESOURCE_STATE_DEPTH_READ;
        case RHIResourceState::CopySource:        return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case RHIResourceState::CopyDest:          return D3D12_RESOURCE_STATE_COPY_DEST;
        case RHIResourceState::Present:           return D3D12_RESOURCE_STATE_PRESENT;
        default:                                  return D3D12_RESOURCE_STATE_COMMON;
        }
    }
}