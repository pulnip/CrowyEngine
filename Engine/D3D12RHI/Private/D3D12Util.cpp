#include "D3D12Util.hpp"

namespace Crowy
{
    DXGI_FORMAT convertTextureFormat(RHITextureFormat format){
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
        case RHITextureFormat::D16_UNORM:         return DXGI_FORMAT_D16_UNORM;
        case RHITextureFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case RHITextureFormat::D32_FLOAT:         return DXGI_FORMAT_D32_FLOAT;
        case RHITextureFormat::D32_FLOAT_S8_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        // Compressed formats
        case RHITextureFormat::BC1_UNORM:         return DXGI_FORMAT_BC1_UNORM;
        case RHITextureFormat::BC1_UNORM_SRGB:    return DXGI_FORMAT_BC1_UNORM_SRGB;
        case RHITextureFormat::BC2_UNORM:         return DXGI_FORMAT_BC2_UNORM;
        case RHITextureFormat::BC2_UNORM_SRGB:    return DXGI_FORMAT_BC2_UNORM_SRGB;
        case RHITextureFormat::BC3_UNORM:         return DXGI_FORMAT_BC3_UNORM;
        case RHITextureFormat::BC3_UNORM_SRGB:    return DXGI_FORMAT_BC3_UNORM_SRGB;
        case RHITextureFormat::BC4_UNORM:         return DXGI_FORMAT_BC4_UNORM;
        case RHITextureFormat::BC4_SNORM:         return DXGI_FORMAT_BC4_SNORM;
        case RHITextureFormat::BC5_UNORM:         return DXGI_FORMAT_BC5_UNORM;
        case RHITextureFormat::BC5_SNORM:         return DXGI_FORMAT_BC5_SNORM;
        case RHITextureFormat::BC6H_UF16:         return DXGI_FORMAT_BC6H_UF16;
        case RHITextureFormat::BC6H_SF16:         return DXGI_FORMAT_BC6H_SF16;
        case RHITextureFormat::BC7_UNORM:         return DXGI_FORMAT_BC7_UNORM;
        case RHITextureFormat::BC7_UNORM_SRGB:    return DXGI_FORMAT_BC7_UNORM_SRGB;

        default:                                  return DXGI_FORMAT_UNKNOWN;
        }
    }

    D3D12_RESOURCE_STATES convertResourceState(RHIResourceState state){
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

    D3D12_COMPARISON_FUNC convertComparisonFunc(RHIComparisonFunc func){
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

    D3D12_BLEND convertBlend(RHIBlend blend){
        switch(blend){
        case RHIBlend::Zero:           return D3D12_BLEND_ZERO;
        case RHIBlend::One:            return D3D12_BLEND_ONE;
        case RHIBlend::SrcColor:       return D3D12_BLEND_SRC_COLOR;
        case RHIBlend::InvSrcColor:    return D3D12_BLEND_INV_SRC_COLOR;
        case RHIBlend::SrcAlpha:       return D3D12_BLEND_SRC_ALPHA;
        case RHIBlend::InvSrcAlpha:    return D3D12_BLEND_INV_SRC_ALPHA;
        case RHIBlend::DestAlpha:      return D3D12_BLEND_DEST_ALPHA;
        case RHIBlend::InvDestAlpha:   return D3D12_BLEND_INV_DEST_ALPHA;
        case RHIBlend::DestColor:      return D3D12_BLEND_DEST_COLOR;
        case RHIBlend::InvDestColor:   return D3D12_BLEND_INV_DEST_COLOR;
        case RHIBlend::SrcAlphaSat:    return D3D12_BLEND_SRC_ALPHA_SAT;
        case RHIBlend::BlendFactor:    return D3D12_BLEND_BLEND_FACTOR;
        case RHIBlend::InvBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;
        default:                       return D3D12_BLEND_ZERO;
        }
    }

    D3D12_BLEND_OP convertBlendOp(RHIBlendOp op){
        switch(op){
        case RHIBlendOp::Add:             return D3D12_BLEND_OP_ADD;
        case RHIBlendOp::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
        case RHIBlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case RHIBlendOp::Min:             return D3D12_BLEND_OP_MIN;
        case RHIBlendOp::Max:             return D3D12_BLEND_OP_MAX;
        default:                          return D3D12_BLEND_OP_ADD;
        }
    }

    D3D12_CULL_MODE convertCullMode(RHICullMode mode){
        switch(mode){
        case RHICullMode::CullNone: return D3D12_CULL_MODE_NONE;
        case RHICullMode::Front:    return D3D12_CULL_MODE_FRONT;
        case RHICullMode::Back:     return D3D12_CULL_MODE_BACK;
        default:                    return D3D12_CULL_MODE_NONE;
        }
    }

    D3D12_FILL_MODE convertFillMode(RHIFillMode mode){
        switch(mode){
        case RHIFillMode::Solid:     return D3D12_FILL_MODE_SOLID;
        case RHIFillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
        default:                     return D3D12_FILL_MODE_SOLID;
        }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE convertTopologyType(RHIPrimitiveTopology topology){
        switch(topology){
        case RHIPrimitiveTopology::PointList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case RHIPrimitiveTopology::LineList:      [[fallthrough]];
        case RHIPrimitiveTopology::LineStrip:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case RHIPrimitiveTopology::TriangleList:  [[fallthrough]];
        case RHIPrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default:                                  return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    }

    D3D_PRIMITIVE_TOPOLOGY convertTopology(RHIPrimitiveTopology topology){
        switch(topology){
        case RHIPrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case RHIPrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case RHIPrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case RHIPrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case RHIPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:                                  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }
}