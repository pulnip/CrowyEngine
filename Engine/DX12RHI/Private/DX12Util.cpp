#include "DX12Util.hpp"

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "PathUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convert(RHIPixelFormat format) {
        using enum RHIPixelFormat;

        switch(format) {
        case Unknown:
            return DXGI_FORMAT_UNKNOWN;
        // 8-bit formats
        case R8_UNORM:
            return DXGI_FORMAT_R8_UNORM;
        case R8_SNORM:
            return DXGI_FORMAT_R8_SNORM;
        case R8_UINT:
            return DXGI_FORMAT_R8_UINT;
        case R8_SINT:
            return DXGI_FORMAT_R8_SINT;
        // 16-bit formats
        case R16_UNORM:
            return DXGI_FORMAT_R16_UNORM;
        case R16_SNORM:
            return DXGI_FORMAT_R16_SNORM;
        case R16_UINT:
            return DXGI_FORMAT_R16_UINT;
        case R16_SINT:
            return DXGI_FORMAT_R16_SINT;
        case R16_FLOAT:
            return DXGI_FORMAT_R16_FLOAT;

        case RG8_UNORM:
            return DXGI_FORMAT_R8G8_UNORM;
        case RG8_SNORM:
            return DXGI_FORMAT_R8G8_SNORM;
        case RG8_UINT:
            return DXGI_FORMAT_R8G8_UINT;
        case RG8_SINT:
            return DXGI_FORMAT_R8G8_SINT;
        // 32-bit formats
        case R32_UINT:
            return DXGI_FORMAT_R32_UINT;
        case R32_SINT:
            return DXGI_FORMAT_R32_SINT;
        case R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;

        case RG16_UNORM:
            return DXGI_FORMAT_R16G16_UNORM;
        case RG16_SNORM:
            return DXGI_FORMAT_R16G16_SNORM;
        case RG16_UINT:
            return DXGI_FORMAT_R16G16_UINT;
        case RG16_SINT:
            return DXGI_FORMAT_R16G16_SINT;
        case RG16_FLOAT:
            return DXGI_FORMAT_R16G16_FLOAT;

        case RGBA8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case RGBA8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case RGBA8_SNORM:
            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case RGBA8_UINT:
            return DXGI_FORMAT_R8G8B8A8_UINT;
        case RGBA8_SINT:
            return DXGI_FORMAT_R8G8B8A8_SINT;

        case BGRA8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case BGRA8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        // 64-bit formats
        case RG32_UINT:
            return DXGI_FORMAT_R32G32_UINT;
        case RG32_SINT:
            return DXGI_FORMAT_R32G32_SINT;
        case RG32_FLOAT:
            return DXGI_FORMAT_R32G32_FLOAT;

        // 96-bit formats
        case RGB32_FLOAT:
            return DXGI_FORMAT_R32G32B32_FLOAT;

        case RGBA16_UNORM:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case RGBA16_SNORM:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case RGBA16_UINT:
            return DXGI_FORMAT_R16G16B16A16_UINT;
        case RGBA16_SINT:
            return DXGI_FORMAT_R16G16B16A16_SINT;
        case RGBA16_FLOAT:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;

        // 128-bit formats
        case RGBA32_UINT:
            return DXGI_FORMAT_R32G32B32A32_UINT;
        case RGBA32_SINT:
            return DXGI_FORMAT_R32G32B32A32_SINT;
        case RGBA32_FLOAT:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;

        // Depth/stencil formats
        case D16_UNORM:
            return DXGI_FORMAT_D16_UNORM;
        case D24_UNORM_S8_UINT:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case D32_FLOAT:
            return DXGI_FORMAT_D32_FLOAT;
        case D32_FLOAT_S8_UINT:
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        // Block-compressed formats
        case BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM;
        case BC1_UNORM_SRGB:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case BC2_UNORM:
            return DXGI_FORMAT_BC2_UNORM;
        case BC2_UNORM_SRGB:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM;
        case BC3_UNORM_SRGB:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case BC4_UNORM:
            return DXGI_FORMAT_BC4_UNORM;
        case BC4_SNORM:
            return DXGI_FORMAT_BC4_SNORM;
        case BC5_UNORM:
            return DXGI_FORMAT_BC5_UNORM;
        case BC5_SNORM:
            return DXGI_FORMAT_BC5_SNORM;
        case BC6H_UF16:
            return DXGI_FORMAT_BC6H_UF16;
        case BC6H_SF16:
            return DXGI_FORMAT_BC6H_SF16;
        case BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM;
        case BC7_UNORM_SRGB:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        default:
            std::unreachable();
        }
    }

    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc func) {
        using enum RHIComparisonFunc;

        switch(func) {
        case Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            std::unreachable();
        }
    }

    D3D12_BARRIER_SYNC convert(RHIBarrierSync state) {
        using enum RHIBarrierSync;

        auto result = D3D12_BARRIER_SYNC_NONE;
        const auto mapBit = [&](RHIBarrierSync rhi, D3D12_BARRIER_SYNC dx) {
            if(hasFlag(state, rhi)) {
                result |= dx;
            }
        };
        mapBit(All, D3D12_BARRIER_SYNC_ALL);
        mapBit(Draw, D3D12_BARRIER_SYNC_DRAW);
        mapBit(VertexShading, D3D12_BARRIER_SYNC_VERTEX_SHADING);
        mapBit(PixelShading, D3D12_BARRIER_SYNC_PIXEL_SHADING);
        mapBit(DepthStencil, D3D12_BARRIER_SYNC_DEPTH_STENCIL);
        mapBit(RenderTarget, D3D12_BARRIER_SYNC_RENDER_TARGET);
        mapBit(Compute, D3D12_BARRIER_SYNC_COMPUTE_SHADING);
        mapBit(Copy, D3D12_BARRIER_SYNC_COPY);
        mapBit(IndexInput, D3D12_BARRIER_SYNC_INDEX_INPUT);
        mapBit(ExecuteIndirect, D3D12_BARRIER_SYNC_EXECUTE_INDIRECT);

        return result;
    }

    D3D12_BARRIER_ACCESS convert(RHIBarrierAccess state) {
        using enum RHIBarrierAccess;

        if(state == NoAccess) {
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        }
        CROWY_ASSERT(
            !hasFlag(state, NoAccess),
            "NoAccess cannot combine with other access bits"
        );

        // Common == 0 == D3D12_BARRIER_ACCESS_COMMON
        auto result = D3D12_BARRIER_ACCESS_COMMON;
        const auto mapBit = [&](RHIBarrierAccess rhi, D3D12_BARRIER_ACCESS dx) {
            if(hasFlag(state, rhi)) {
                result |= dx;
            }
        };
        mapBit(VertexBuffer, D3D12_BARRIER_ACCESS_VERTEX_BUFFER);
        mapBit(IndexBuffer, D3D12_BARRIER_ACCESS_INDEX_BUFFER);
        mapBit(UniformBuffer, D3D12_BARRIER_ACCESS_CONSTANT_BUFFER);
        mapBit(ShaderResource, D3D12_BARRIER_ACCESS_SHADER_RESOURCE);
        mapBit(UnorderedAccess, D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);
        mapBit(RenderTarget, D3D12_BARRIER_ACCESS_RENDER_TARGET);
        mapBit(DepthWrite, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE);
        mapBit(DepthRead, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ);
        mapBit(IndirectArgs, D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT);
        mapBit(CopySrc, D3D12_BARRIER_ACCESS_COPY_SOURCE);
        mapBit(CopyDst, D3D12_BARRIER_ACCESS_COPY_DEST);

        return result;
    }

    D3D12_BARRIER_LAYOUT convert(RHITextureLayout state) {
        using enum RHITextureLayout;

        switch(state) {
        case Undefined:
            return D3D12_BARRIER_LAYOUT_UNDEFINED;
        case Common:
            return D3D12_BARRIER_LAYOUT_COMMON;
        case RenderTarget:
            return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
        case DepthWrite:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
        case DepthRead:
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        case ShaderResource:
            return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
        case UnorderedAccess:
            return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
        case CopySrc:
            return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
        case CopyDst:
            return D3D12_BARRIER_LAYOUT_COPY_DEST;
        case Present:
            return D3D12_BARRIER_LAYOUT_PRESENT;
        default:
            std::unreachable();
        }
    }

    Str HResultToString(HRESULT hr) {
        wchar_t* message = nullptr;
        const auto length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(hr),
            0,
            reinterpret_cast<wchar_t*>(&message),
            0,
            nullptr
        );
        if(length == 0)
            return std::format("HRESULT 0x{:08X}", static_cast<u32>(hr));

        auto text = fromUTF16String(std::wstring_view(message, length));
        LocalFree(message);
        while(!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();

        return text;
    }
}
