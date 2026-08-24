#include <d3dx12/d3dx12_core.h>
#include "Assert.hpp"
#include "DescriptorHeapAllocator.hpp"
#include "DX12Definitions.hpp"
#include "EnumUtil.hpp"
#include "RHIDefinitions.hpp"
#include "DX12Texture.hpp"
#include "DX12Util.hpp"
#include "VariantUtil.hpp"

namespace{
    DXGI_FORMAT toPhysicalFormat(
        Crowy::RHIPixelFormat format,
        bool isShaderResource
    ){
        using namespace Crowy;
        using enum RHIPixelFormat;

        if(isShaderResource){
            switch(format){
            case D16_UNORM:         return DXGI_FORMAT_R16_TYPELESS;
            case D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
            case D32_FLOAT:         return DXGI_FORMAT_R32_TYPELESS;
            case D32_FLOAT_S8_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;
            default:
                break;
            }
        }

        return convert(format);
    }
}

namespace Crowy
{
    DX12Texture::DX12Texture(
        DX12Allocator& allocator,
        const RHITextureCreateDesc& desc,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        StrView name
    )
        : RHITexture(
            desc.format,
            desc.mipLevels,
            desc.arraySize
        )
        , allocator(&allocator)
        , cbvsrvuavHeap(cbvsrvuavHeap)
        , rtvHeap(rtvHeap)
        , dsvHeap(dsvHeap)
    {
        using enum RHITextureUsage;
        using enum RHIMemoryAccess;

        const auto isShaderResource  = hasFlag(desc.usage, ShaderResource);
        const auto isUnorderedAccess = hasFlag(desc.usage, UnorderedAccess);
        const auto isRenderTarget    = hasFlag(desc.usage, RenderTarget);
        const auto isDepthStencil    = hasFlag(desc.usage, DepthStencil);

        CROWY_ASSERT(!IsBlockCompressed(desc.format) || isShaderResource,
            "Block-compressed textures are shader-resource only"
        );
        CROWY_ASSERT(!IsDepthFormat(desc.format) || isDepthStencil,
            "Depth format should be depth stencil usage"
        );

        const auto is3D = desc.depth > 1;
        CROWY_ASSERT(!is3D || desc.arraySize == 1,
            "3D textures cannot be arrayed"
        );
        // the RTV/DSV paths only build TEXTURE2D views
        CROWY_ASSERT(!is3D || (!isRenderTarget && !isDepthStencil),
            "3D render/depth targets are not supported"
        );

        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if(isRenderTarget)    flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if(isDepthStencil)    flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if(isUnorderedAccess) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if(!isShaderResource) flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

        const auto dxFormat = toPhysicalFormat(desc.format, isShaderResource);
        const auto texDesc = is3D ?
            CD3DX12_RESOURCE_DESC1::Tex3D(
                dxFormat,
                desc.width,
                desc.height,
                desc.depth,
                desc.mipLevels,
                flags
            ) :
            CD3DX12_RESOURCE_DESC1::Tex2D(
                dxFormat,
                desc.width,
                desc.height,
                desc.arraySize,
                desc.mipLevels,
                1, 0,
                flags
            );

        D3D12_CLEAR_VALUE clearValue{
            .Format = dxFormat
        };
        D3D12_CLEAR_VALUE* pClearValue = nullptr;
        if(isRenderTarget && !isDepthStencil){
            clearValue.Color[0] = desc.clearColor.x;
            clearValue.Color[1] = desc.clearColor.y;
            clearValue.Color[2] = desc.clearColor.z;
            clearValue.Color[3] = desc.clearColor.w;
            pClearValue = &clearValue;
        }
        else if(!isRenderTarget && isDepthStencil){
            clearValue.DepthStencil.Depth = desc.clearDepthStencil.depth;
            clearValue.DepthStencil.Stencil = desc.clearDepthStencil.stencil;
            pClearValue = &clearValue;
        }

        allocation = allocator.Allocate(
            texDesc,
            RHIMemoryClass::Device,
            pClearValue,
            name
        );
        texture = allocation.resource;
    }

    DX12Texture::DX12Texture(
        Swapchain& swapchain,
        RHIPixelFormat logicalFormat,
        UINT bufferIndex,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        StrView name
    )
        // the swapchain hands buffers over in the Present layout
        : RHITexture(logicalFormat, 1, 1)
        , cbvsrvuavHeap(cbvsrvuavHeap)
        , rtvHeap(rtvHeap)
        , dsvHeap(dsvHeap)
    {
        CHECK_HRESULT(swapchain.GetBuffer(
            bufferIndex,
            IID_PPV_ARGS(&backBufferOwner)
        ), "Failed to Get Buffer from Swapchain");
        texture = backBufferOwner.Get();

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            texture->SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()),
                name.data()
            );
        }
    #endif
    }

    DX12Texture::~DX12Texture(){
        for(const auto& [_, idx]: srvs){
            cbvsrvuavHeap.Free(idx);
        }
        for(const auto& [_, idx]: rtvs){
            rtvHeap.Free(idx);
        }
        for(const auto& [_, idx]: uavs){
            cbvsrvuavHeap.Free(idx);
        }
        for(const auto& [_, idx]: dsvs){
            dsvHeap.Free(idx);
        }

        texture = nullptr;
        if(allocator != nullptr){
            allocator->Free(allocation);
        }
    }

    u32 DX12Texture::GetWidth() const noexcept{
        const auto desc = texture->GetDesc();
        return desc.Width;
    }
    u32 DX12Texture::GetHeight() const noexcept{
        const auto desc = texture->GetDesc();
        return desc.Height;
    }
    u32 DX12Texture::GetDepth() const noexcept{
        const auto desc = texture->GetDesc();
        // DepthOrArraySize holds the array size for non-3D textures
        return desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ?
            desc.DepthOrArraySize : 1;
    }

    UINT DX12Texture::GetOrCreateRTV(const RHITextureViewDesc& desc){
        if(auto it = rtvs.find(desc); it != rtvs.end())
            return it->second;

        const D3D12_RENDER_TARGET_VIEW_DESC dxDesc{
            .Format = convert(desc.format),
            .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
            .Texture2D = D3D12_TEX2D_RTV{
                // a render target is a single mip, so mipCount plays no part
                .MipSlice = desc.mostDetailedMip,
                .PlaneSlice = 0
            }
        };

        auto idx = rtvHeap.Allocate(
            *texture,
            dxDesc
        );
        auto [it, ret] = rtvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    UINT DX12Texture::GetOrCreateDSV(const RHITextureViewDesc& desc){
        if(auto it = dsvs.find(desc); it != dsvs.end())
            return it->second;

        const D3D12_DEPTH_STENCIL_VIEW_DESC dxDesc{
            .Format = convert(desc.format),
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D = D3D12_TEX2D_DSV{
                // a depth target is a single mip, so mipCount plays no part
                .MipSlice = desc.mostDetailedMip
            }
        };

        auto idx = dsvHeap.Allocate(
            *texture,
            dxDesc
        );
        auto [it, ret] = dsvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    u64 DX12Texture::GetReadableID(const RHITextureViewDesc& desc){
        if(auto it = srvs.find(desc); it != srvs.end())
            return it->second;

        // RHI_ALL_MIPS is the same all-ones value D3D12 reads as "the rest"
        const auto dxDesc = std::visit(overload{
            [&](const RHITextureViewDesc::Tex2D&){
                return CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2D(
                    convert(desc.format),
                    desc.mipCount,
                    desc.mostDetailedMip
                );
            },
            [&](const RHITextureViewDesc::TexCube&){
                return CD3DX12_SHADER_RESOURCE_VIEW_DESC::TexCube(
                    convert(desc.format),
                    desc.mipCount,
                    desc.mostDetailedMip
                );
            },
            [&](const RHITextureViewDesc::Tex3D&){
                return CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex3D(
                    convert(desc.format),
                    desc.mipCount,
                    desc.mostDetailedMip
                );
            }
        }, desc.config);

        auto idx = cbvsrvuavHeap.Allocate(
            *texture,
            dxDesc
        );
        auto [it, ret] = srvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    u64 DX12Texture::GetWritableID(const RHITextureViewDesc& desc){
        if(auto it = uavs.find(desc); it != uavs.end())
            return it->second;

        // unordered access binds a single mip, so mipCount plays no part
        const auto dxDesc = std::visit(overload{
            [&](const RHITextureViewDesc::Tex2D&){
                return CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2D(
                    convert(desc.format),
                    desc.mostDetailedMip
                );
            },
            [&](const RHITextureViewDesc::TexCube&){
                CROWY_ASSERT(false,
                    "cube unordered access views do not exist"
                );
                return CD3DX12_UNORDERED_ACCESS_VIEW_DESC{};
            },
            [&](const RHITextureViewDesc::Tex3D&){
                // -1 binds every W slice of the mip
                return CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex3D(
                    convert(desc.format),
                    UINT(-1),
                    0,
                    desc.mostDetailedMip
                );
            }
        }, desc.config);

        auto idx = cbvsrvuavHeap.Allocate(
            *texture,
            dxDesc
        );
        auto [it, ret] = uavs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }
}
