#pragma once

#include <cstddef>
#include <unordered_map>
#include <d3d11.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif
#include "D3D11Definitions.hpp"
#include "D3D11Util.hpp"

namespace Crowy
{
    class D3D11Texture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        TextureRAII texture = nullptr;
        size_t width, height;
        RHIPixelFormat format = RHIPixelFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;

        Device& device;
        DeviceContext& context;

        std::unordered_map<RHITextureViewDesc, SRVRAII> srvs;
        std::unordered_map<RHITextureViewDesc, RTVRAII> rtvs;
        std::unordered_map<RHITextureViewDesc, UAVRAII> uavs;
        std::unordered_map<RHITextureViewDesc, DSVRAII> dsvs;

    public:
        D3D11Texture(
            ID3D11Device& device,
            ID3D11DeviceContext& context,
            const RHITextureCreateDesc& desc,
            const std::string& name
        )
            : device(device), context(context)
            , width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
        {
            using enum RHITextureUsage;

            CROWY_ASSERT(desc.depth == 1);

            auto isShaderResource  = has_flag(desc.usage, AllowShaderRead);
            auto isRenderTarget    = has_flag(desc.usage, AllowRenderTarget);
            auto isDepthTarget     = has_flag(desc.usage, AllowDepthStencil);
            auto isUnorderedAccess = has_flag(desc.usage, AllowShaderWrite);

            UINT bindFlags = 0;
            if(isShaderResource ) bindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if(isRenderTarget   ) bindFlags |= D3D11_BIND_RENDER_TARGET;
            if(isDepthTarget    ) bindFlags |= D3D11_BIND_DEPTH_STENCIL;
            if(isUnorderedAccess) bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            bool needsGPUOnly = isShaderResource || isRenderTarget || 
                                isDepthTarget    || isUnorderedAccess;

            D3D11_TEXTURE2D_DESC texDesc{
                .Width = desc.width,
                .Height = desc.height,
                .MipLevels = desc.mipLevels,
                .ArraySize = desc.arraySize,
                .Format = convertPixelFormat(desc.format, isShaderResource, isDepthTarget),
                // No MSAA
                .SampleDesc = {1, 0},
                .Usage = needsGPUOnly ?
                    D3D11_USAGE_DEFAULT : D3D11_USAGE_STAGING,
                .BindFlags = bindFlags,
                .CPUAccessFlags = needsGPUOnly ?
                    UINT(0) : D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE,
                .MiscFlags = 0
            };
            D3D11_SUBRESOURCE_DATA initData{
                .pSysMem = desc.initialData,
                .SysMemPitch = desc.width * static_cast<UINT>(getBytesPerPixel(desc.format)),
                .SysMemSlicePitch = 0
            };

            if(FAILED(device.CreateTexture2D(
                &texDesc,
                desc.initialData != nullptr ? &initData : nullptr,
                &texture
            ))){
                throw std::runtime_error("Failed to create D3D11 texture");
            }
        #if defined(_DEBUG) || !defined(NDEBUG)
            if(!name.empty()){
                texture->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(name.length()),
                    name.c_str()
                );
            }
        #endif
        }

        ~D3D11Texture() = default;

        void upload(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            // TODO
        }

        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }
        size_t getWidth() const noexcept RHI_OVERRIDE{
            return width;
        }
        size_t getHeight() const noexcept RHI_OVERRIDE{
            return height;
        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }

        Texture* get() const{ return texture.Get(); }

        SRV* getOrCreateSRV(const RHITextureViewDesc& rhiDesc){
            if(auto it = srvs.find(rhiDesc); it != srvs.end())
                return it->second.Get();

            const D3D11_SHADER_RESOURCE_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format, true, false),
                .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MostDetailedMip = 0,
                    .MipLevels = 1
                }
            };

            SRVRAII view;
            device.CreateShaderResourceView(
                texture.Get(),
                &desc,
                &view
            );

            auto [it, r] = srvs.emplace(rhiDesc, std::move(view));
            CROWY_ASSERT(r);

            return it->second.Get();
        }

        RTV* getOrCreateRTV(const RHITextureViewDesc& rhiDesc){
            if(auto it = rtvs.find(rhiDesc); it != rtvs.end())
                return it->second.Get();

            const D3D11_RENDER_TARGET_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format),
                .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0
                }
            };

            RTVRAII view;
            device.CreateRenderTargetView(
                texture.Get(),
                &desc,
                &view
            );

            auto [it, r] = rtvs.emplace(rhiDesc, std::move(view));
            CROWY_ASSERT(r);

            return it->second.Get();
        }

        UAV* getOrCreateUAV(const RHITextureViewDesc& rhiDesc){
            if(auto it = uavs.find(rhiDesc); it != uavs.end())
                return it->second.Get();

            const D3D11_UNORDERED_ACCESS_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format),
                .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0
                }
            };

            UAVRAII view;
            device.CreateUnorderedAccessView(
                texture.Get(),
                &desc,
                &view
            );

            auto [it, r] = uavs.emplace(rhiDesc, std::move(view));
            CROWY_ASSERT(r);

            return it->second.Get();
        }

        DSV* getOrCreateDSV(const RHITextureViewDesc& rhiDesc){
            if(auto it = dsvs.find(rhiDesc); it != dsvs.end())
                return it->second.Get();

            const D3D11_DEPTH_STENCIL_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format, false, true),
                .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0
                }
            };

            DSVRAII view;
            device.CreateDepthStencilView(
                texture.Get(),
                &desc,
                &view
            );

            auto [it, r] = dsvs.emplace(rhiDesc, std::move(view));
            CROWY_ASSERT(r);

            return it->second.Get();
        }
    };
}
