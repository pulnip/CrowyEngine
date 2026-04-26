#pragma once

#include <d3d11.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBufferView.hpp"
#endif

namespace Crowy
{
    class D3D11BufferSRV
#ifndef USE_STATIC_RHI
        : public RHIBufferView
#endif
    {
    private:
        ID3D11ShaderResourceView* view = nullptr;
        RHIBindingAccess access;
        uint32_t offset = 0, size = 0;

    public:
        D3D11BufferSRV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHIBufferViewDesc& desc,
            const std::string& name
        )
            : access(desc.access)
            , offset(desc.offset), size(desc.size)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(desc.access, ReadOnly));

            const D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
                .Format = DXGI_FORMAT_R32_FLOAT,
                .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
                .Buffer = {

                }
            };
            device.CreateShaderResourceView(
                &resource,
                &srvDesc,
                &view
            );
        }

        ~D3D11BufferSRV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        uint32_t getOffset() const noexcept RHI_OVERRIDE{
            return offset;
        }
        uint32_t getSize() const noexcept RHI_OVERRIDE{
            return size;
        }

        ID3D11ShaderResourceView* get() const noexcept{
            return view;
        }
    };

    class D3D11BufferRTV
#ifndef USE_STATIC_RHI
        : public RHIBufferView
#endif
    {
    private:
        ID3D11RenderTargetView* view = nullptr;
        RHIBindingAccess access;
        uint32_t offset = 0, size = 0;

    public:
        D3D11BufferRTV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHIBufferViewDesc& desc,
            const std::string& name
        )
            : access(desc.access)
            , offset(desc.offset), size(desc.size)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(desc.access, WriteOnly));

            const D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{
                .Format = DXGI_FORMAT_R32_FLOAT,
                .ViewDimension = D3D11_RTV_DIMENSION_BUFFER,
                .Buffer = {

                }
            };
            device.CreateRenderTargetView(
                &resource,
                &rtvDesc,
                &view
            );
        }

        ~D3D11BufferRTV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        uint32_t getOffset() const noexcept RHI_OVERRIDE{
            return offset;
        }
        uint32_t getSize() const noexcept RHI_OVERRIDE{
            return size;
        }

        ID3D11RenderTargetView* get() const noexcept{
            return view;
        }
    };

    class D3D11BufferUAV
#ifndef USE_STATIC_RHI
        : public RHIBufferView
#endif
    {
    private:
        ID3D11UnorderedAccessView* view = nullptr;
        RHIBindingAccess access;
        uint32_t offset = 0, size = 0;

    public:
        D3D11BufferUAV(
            ID3D11Device& device,
            ID3D11Resource& resource,
            const RHIBufferViewDesc& desc,
            const std::string& name
        )
            : access(desc.access)
            , offset(desc.offset), size(desc.size)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(desc.access, ReadWrite));

            const D3D11_UNORDERED_ACCESS_VIEW_DESC rtvDesc{
                .Format = DXGI_FORMAT_R32_FLOAT,
                .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
                .Buffer = {

                }
            };
            device.CreateUnorderedAccessView(
                &resource,
                &rtvDesc,
                &view
            );
        }

        ~D3D11BufferUAV(){
            if(view != nullptr){
                view->Release();
                view = nullptr;
            }
        }

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        uint32_t getOffset() const noexcept RHI_OVERRIDE{
            return offset;
        }
        uint32_t getSize() const noexcept RHI_OVERRIDE{
            return size;
        }

        ID3D11UnorderedAccessView* get() const noexcept{
            return view;
        }
    };
}