#pragma once

#include <cstddef>
#include <d3d11.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif
#include "D3D11Util.hpp"

namespace Crowy
{
    class D3D11Texture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        ID3D11Texture2D* texture = nullptr;
        ID3D11DeviceContext* context = nullptr;
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;

    public:
        D3D11Texture(
            ID3D11Device* device,
            ID3D11DeviceContext* context,
            const RHITextureCreateDesc& desc,
            const std::string& name
        )
            : context(context)
            , width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
        {
            CROWY_ASSERT(desc.depth == 1);

            auto isShaderResource  = has_flag(desc.usage, RHITextureUsage::ShaderResource);
            auto isRenderTarget    = has_flag(desc.usage, RHITextureUsage::RenderTarget);
            auto isDepthTarget     = has_flag(desc.usage, RHITextureUsage::DepthStencil);
            auto isUnorderedAccess = has_flag(desc.usage, RHITextureUsage::UnorderedAccess);

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
                .Format = convertTextureFormat(desc.format, isShaderResource, isDepthTarget),
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

            if(FAILED(device->CreateTexture2D(
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

            if(isRenderTarget){
                D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{
                    .Format = convertTextureFormat(desc.format),
                    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        .MipSlice = 0
                    }
                };
                device->CreateRenderTargetView(texture, &rtvDesc, &rtv);
            }
            if(isShaderResource){
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
                    .Format = convertTextureFormat(desc.format, true, false),
                    .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        .MostDetailedMip = 0,
                        .MipLevels = 1
                    }
                };
                device->CreateShaderResourceView(texture, &srvDesc, &srv);
            }
            if(isDepthTarget){
                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{
                    .Format = convertTextureFormat(desc.format, false, true),
                    .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        .MipSlice = 0
                    }
                };
                device->CreateDepthStencilView(texture, &dsvDesc, &dsv);
            }
        }

        ~D3D11Texture(){
            if(rtv != nullptr){
                rtv->Release();
                rtv = nullptr;
            }
            if(srv != nullptr){
                srv->Release();
                srv = nullptr;
            }
            if(dsv != nullptr){
                dsv->Release();
                dsv = nullptr;
            }
            if(texture != nullptr){
                texture->Release();
                texture = nullptr;
            }
        }

        void upload(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            // TODO
        }

        RHITextureFormat getFormat() const noexcept RHI_OVERRIDE{
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

        ID3D11Texture2D* get() const{ return texture; }
        ID3D11RenderTargetView* getRTV() const{ return rtv; }
        ID3D11ShaderResourceView* getSRV() const{ return srv; }
        ID3D11DepthStencilView* getDSV() const{ return dsv; }
    };
}
