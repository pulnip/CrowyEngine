#pragma once

#include <d3d11.h>
#include <stdexcept>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "type_traits.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBufferView.hpp"
#endif
#include "D3D11Util.hpp"

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
            const RHIBufferViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , offset(rhiDesc.offset), size(rhiDesc.size)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(rhiDesc.access, ReadOnly));

            const auto desc = std::visit(overload{
                [&rhiDesc](const RHIBufferViewDesc::RawConfig&){
                    constexpr uint32_t RAW_STRIDE = 4;
                    return D3D11_SHADER_RESOURCE_VIEW_DESC{
                        .Format = DXGI_FORMAT_R32_TYPELESS,
                        .ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX,
                        .BufferEx = {
                            .FirstElement = rhiDesc.offset / RAW_STRIDE,
                            .NumElements = rhiDesc.size / RAW_STRIDE,
                            .Flags = D3D11_BUFFEREX_SRV_FLAG_RAW,
                        }
                    };
                },
                [&rhiDesc](const RHIBufferViewDesc::TypedConfig& c){
                    const uint32_t bpp = getBytesPerPixel(c.format);
                    return D3D11_SHADER_RESOURCE_VIEW_DESC{
                        .Format = convertPixelFormat(c.format),
                        .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
                        .Buffer = {
                            .FirstElement = rhiDesc.offset / bpp,
                            .NumElements = rhiDesc.size / bpp,
                        },
                    };
                },
                [rhiDesc](const RHIBufferViewDesc::StructuredConfig& c){
                    return D3D11_SHADER_RESOURCE_VIEW_DESC{
                        .Format = DXGI_FORMAT_UNKNOWN,
                        .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
                        .Buffer = {
                            .FirstElement = rhiDesc.offset / c.stride,
                            .NumElements = rhiDesc.size / c.stride,
                        },
                    };
                }
            }, rhiDesc.config);
            device.CreateShaderResourceView(
                &resource,
                &desc,
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
            const RHIBufferViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , offset(rhiDesc.offset), size(rhiDesc.size)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(rhiDesc.access, WriteOnly));

            throw std::runtime_error("Unimplemented");
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
            const RHIBufferViewDesc& rhiDesc,
            const std::string& name
        )
            : access(rhiDesc.access)
            , offset(rhiDesc.offset), size(rhiDesc.size)
        {
            using enum RHIBindingAccess;
            CROWY_ASSERT(has_flag(rhiDesc.access, ReadWrite));

            const auto desc = std::visit(overload{
                [&rhiDesc](const RHIBufferViewDesc::RawConfig&){
                    constexpr uint32_t RAW_STRIDE = 4;
                    return D3D11_UNORDERED_ACCESS_VIEW_DESC{
                        .Format = DXGI_FORMAT_R32_TYPELESS,
                        .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
                        .Buffer = {
                            .FirstElement = rhiDesc.offset / RAW_STRIDE,
                            .NumElements = rhiDesc.size / RAW_STRIDE,
                            .Flags = D3D11_BUFFER_UAV_FLAG_RAW
                        }
                    };
                },
                [&rhiDesc](const RHIBufferViewDesc::TypedConfig& c){
                    const uint32_t bpp = getBytesPerPixel(c.format);
                    return D3D11_UNORDERED_ACCESS_VIEW_DESC{
                        .Format = convertPixelFormat(c.format),
                        .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
                        .Buffer = {
                            .FirstElement = rhiDesc.offset / bpp,
                            .NumElements = rhiDesc.size / bpp,
                            .Flags = 0
                        },
                    };
                },
                [rhiDesc](const RHIBufferViewDesc::StructuredConfig& c){
                    return D3D11_UNORDERED_ACCESS_VIEW_DESC{
                        .Format = DXGI_FORMAT_UNKNOWN,
                        .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
                        .Buffer = {
                            .FirstElement = rhiDesc.offset / c.stride,
                            .NumElements = rhiDesc.size / c.stride,
                            .Flags = D3D11_BUFFER_UAV_FLAG_COUNTER
                        },
                    };
                }
            }, rhiDesc.config);

            device.CreateUnorderedAccessView(
                &resource,
                &desc,
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