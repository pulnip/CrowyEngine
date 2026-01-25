#pragma once

#include <cstddef>
#include <memory>
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
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;

    public:
        D3D11Texture(
            ID3D11Device* device,
            const RHITextureCreateDesc& desc
        )
            : width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
        {
            CROWY_ASSERT(desc.depth == 1);

            UINT bindFlags = 0;
            if (hasFlag(desc.usage, RHITextureUsage::ShaderResource))
                bindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if (hasFlag(desc.usage, RHITextureUsage::RenderTarget))
                bindFlags |= D3D11_BIND_RENDER_TARGET;
            if (hasFlag(desc.usage, RHITextureUsage::DepthStencil))
                bindFlags |= D3D11_BIND_DEPTH_STENCIL;
            if (hasFlag(desc.usage, RHITextureUsage::UnorderedAccess))
                bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
            bool needsGPUOnly = hasFlag(desc.usage, RHITextureUsage::RenderTarget) || 
                                hasFlag(desc.usage, RHITextureUsage::DepthStencil) ||
                                hasFlag(desc.usage, RHITextureUsage::ShaderResource) ||
                                hasFlag(desc.usage, RHITextureUsage::UnorderedAccess);
            D3D11_TEXTURE2D_DESC texDesc{
                .Width = desc.width,
                .Height = desc.height,
                .MipLevels = desc.mipLevels,
                .ArraySize = desc.arraySize,
                .Format = convertTextureFormat(desc.format),
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

            if(desc.debugName){
                texture->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(strlen(desc.debugName)),
                    desc.debugName
                );
            }

            if(hasFlag(desc.usage, RHITextureUsage::RenderTarget))
                device->CreateRenderTargetView(texture, nullptr, &rtv);
            if(hasFlag(desc.usage, RHITextureUsage::ShaderResource))
                device->CreateShaderResourceView(texture, nullptr, &srv);
            if(hasFlag(desc.usage, RHITextureUsage::DepthStencil))
                device->CreateDepthStencilView(texture, nullptr, &dsv);
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

        void uploadData(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            // No-Op
        }

        RHITextureFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
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
