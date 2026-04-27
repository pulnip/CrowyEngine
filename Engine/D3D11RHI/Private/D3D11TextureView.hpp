#pragma once

#include <d3d11.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITextureView.hpp"
#endif
#include "D3D11Util.hpp"

namespace Crowy
{
    class D3D11TextureSRV
#ifndef USE_STATIC_RHI
        : public RHITextureView
#endif
    {
    private:
        ID3D11ShaderResourceView* view = nullptr;
        RHIBindingAccess access;
        RHIPixelFormat format;

    public:
        D3D11TextureSRV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHITextureViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , format(rhiDesc.format)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(rhiDesc.access, ReadOnly));

            const D3D11_SHADER_RESOURCE_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format, true, false),
                .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MostDetailedMip = 0,
                    .MipLevels = 1
                }
            };
            device.CreateShaderResourceView(
                &resource,
                &desc,
                &view
            );
        }

        ~D3D11TextureSRV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }

        ID3D11ShaderResourceView* get() const noexcept{
            return view;
        }
    };

    class D3D11TextureRTV
#ifndef USE_STATIC_RHI
        : public RHITextureView
#endif
    {
    private:
        ID3D11RenderTargetView* view = nullptr;
        RHIBindingAccess access;
        RHIPixelFormat format;

    public:
        D3D11TextureRTV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHITextureViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , format(rhiDesc.format)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(rhiDesc.access, WriteOnly));

            const D3D11_RENDER_TARGET_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format),
                .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0
                }
            };
            device.CreateRenderTargetView(
                &resource,
                &desc,
                &view
            );
        }

        ~D3D11TextureRTV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }

        ID3D11RenderTargetView* get() const noexcept{
            return view;
        }
    };

    class D3D11TextureUAV
#ifndef USE_STATIC_RHI
        : public RHITextureView
#endif
    {
    private:
        ID3D11UnorderedAccessView* view = nullptr;
        RHIBindingAccess access;
        RHIPixelFormat format;

    public:
        D3D11TextureUAV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHITextureViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , format(rhiDesc.format)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(rhiDesc.access, ReadWrite));

            const D3D11_UNORDERED_ACCESS_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format),
                .ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0
                }
            };
            device.CreateUnorderedAccessView(
                &resource,
                &desc,
                &view
            );
        }

        ~D3D11TextureUAV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }

        ID3D11UnorderedAccessView* get() const noexcept{
            return view;
        }
    };

    class D3D11TextureDSV
#ifndef USE_STATIC_RHI
        : public RHITextureView
#endif
    {
    private:
        ID3D11DepthStencilView* view = nullptr;
        RHIBindingAccess access;
        RHIPixelFormat format;

    public:
        D3D11TextureDSV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHITextureViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , format(rhiDesc.format)
        {
            const D3D11_DEPTH_STENCIL_VIEW_DESC desc{
                .Format = convertPixelFormat(rhiDesc.format, false, true),
                .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
                .Texture2D = {
                    .MipSlice = 0
                }
            };
            device.CreateDepthStencilView(
                &resource,
                &desc,
                &view
            );
        }

        ~D3D11TextureDSV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }

        ID3D11DepthStencilView* get() const noexcept{
            return view;
        }
    };
}