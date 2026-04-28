#pragma once

#include <stdexcept>
#include <unordered_map>
#include <d3dcommon.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "semantics.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBuffer.hpp"
#endif
#include "D3D11Definitions.hpp"
#include "D3D11Util.hpp"

namespace Crowy
{
    class D3D11Buffer
#ifndef USE_STATIC_RHI
        : public RHIBuffer
#endif
    {
    private:
        BufferRAII buffer = nullptr;
        uint32_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;

        Device& device;
        DeviceContext& context;

        RHIResourceState currentState = RHIResourceState::Common;

        std::unordered_map<RHIBufferViewDesc, SRVRAII> srvs;
        std::unordered_map<RHIBufferViewDesc, UAVRAII> uavs;

    public:
        D3D11Buffer(
            ID3D11Device& device,
            ID3D11DeviceContext& context,
            const RHIBufferCreateDesc& desc,
            const std::string& name
        )
            : device(device), context(context)
            , usage(desc.usage), size(desc.size)
        {
            using enum RHIBufferUsage;
            using enum RHIMemoryAccess;

            const auto hasVertexUsage = has_flag(desc.usage, VertexBuffer);
            const auto hasIndexUsage = has_flag(desc.usage, IndexBuffer);
            const auto hasConstantUsage = has_flag(desc.usage, ConstantBuffer);
            const auto needCPUAccess = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;

            const auto isGPUOnly = has_flag(desc.access, GPUOnly);
            CROWY_ASSERT(!needCPUAccess || !isGPUOnly);

            const auto isShaderResource = has_flag(desc.usage, AllowShaderRead);
            const auto isUnorderedAccess = has_flag(desc.usage, AllowShaderWrite);

            UINT bindFlags = 0;
            if(hasVertexUsage)
                bindFlags |= D3D11_BIND_VERTEX_BUFFER;
            if(hasIndexUsage)
                bindFlags |= D3D11_BIND_INDEX_BUFFER;
            if(hasConstantUsage)
                bindFlags |= D3D11_BIND_CONSTANT_BUFFER;
            if(isShaderResource)
                bindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if(isUnorderedAccess)
                bindFlags |= D3D11_BIND_UNORDERED_ACCESS;

            UINT miscFlags = 0;
            if(has_flag(desc.usage, IndirectArgs))
                miscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

            D3D11_BUFFER_DESC dxDesc = {
                .ByteWidth = static_cast<UINT>(desc.size),
                .Usage = isGPUOnly ?
                    D3D11_USAGE_DEFAULT: D3D11_USAGE_DYNAMIC,
                .BindFlags = bindFlags,
                .CPUAccessFlags = isGPUOnly ?
                    UINT(0) : D3D11_CPU_ACCESS_WRITE,
                .MiscFlags = miscFlags,
                .StructureByteStride = 0
            };

            D3D11_SUBRESOURCE_DATA initData{
                .pSysMem = desc.initialData
            };

            if(FAILED(device.CreateBuffer(
                &dxDesc,
                desc.initialData != nullptr ? &initData : nullptr,
                &buffer
            ))){
                throw std::runtime_error("Failed to create D3D11 buffer");
            }

        #if defined(_DEBUG) || !defined(NDEBUG)
            if(!name.empty()){
                buffer->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(name.length()),
                    name.c_str()
                );
            }
        #endif
        }

        ~D3D11Buffer() = default;

        CROWY_DECLARE_PINNED(D3D11Buffer)

        inline void upload(
            const void* data, uint32_t size,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(size <= bufSize - offset);

            D3D11_MAPPED_SUBRESOURCE mapped;
            context.Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

            std::memcpy(mapped.pData, data, size);

            context.Unmap(buffer.Get(), 0);
        }

        inline void download(
            void* data, uint32_t size,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(size <= bufSize - offset);
            D3D11_MAPPED_SUBRESOURCE mapped;
            context.Map(buffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);

            std::memcpy(data, mapped.pData, size);

            context.Unmap(buffer.Get(), 0);
        }

        uint32_t getSize() const noexcept RHI_OVERRIDE{
            return size;
        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            currentState = state;
        }

        Buffer* get() const{ return buffer.Get(); }

        SRV* getOrCreateSRV(const RHIBufferViewDesc& rhiDesc){
            if(auto it = srvs.find(rhiDesc); it != srvs.end())
                return it->second.Get();

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

            SRVRAII view;
            device.CreateShaderResourceView(
                buffer.Get(),
                &desc,
                &view
            );

            auto [it, r] = srvs.emplace(rhiDesc, std::move(view));
            CROWY_ASSERT(r);

            return it->second.Get();
        }

        UAV* getOrCreateUAV(const RHIBufferViewDesc& rhiDesc){
            if(auto it = uavs.find(rhiDesc); it != uavs.end())
                return it->second.Get();

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

            UAVRAII view;
            device.CreateUnorderedAccessView(
                buffer.Get(),
                &desc,
                &view
            );
            auto [it, r] = uavs.emplace(rhiDesc, std::move(view));
            CROWY_ASSERT(r);

            return it->second.Get();
        }
    };
}
