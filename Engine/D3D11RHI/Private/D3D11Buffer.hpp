#pragma once

#include <cstddef>
#include <d3dcommon.h>
#include <stdexcept>
#include <d3d11.h>
#include "assert.hpp"
#include "enum_traits.hpp"
#include "semantics.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBuffer.hpp"
#endif

namespace Crowy
{
    class D3D11Buffer
#ifndef USE_STATIC_RHI
        : public RHIBuffer
#endif
    {
    private:
        ID3D11Buffer* buffer = nullptr;
        ID3D11DeviceContext* context = nullptr;
        size_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;
        bool isCPUAccessible = false;
        RHIResourceState currentState = RHIResourceState::Common;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11UnorderedAccessView* uav = nullptr;

    public:
        D3D11Buffer(
            ID3D11Device* device,
            ID3D11DeviceContext* context,
            const RHIBufferCreateDesc& desc,
            const std::string& name
        )
            : context(context)
            , usage(desc.usage), size(desc.size)
        {
            auto hasVertexUsage = has_flag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = has_flag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = has_flag(desc.usage, RHIBufferUsage::ConstantBuffer);
            auto hasCPUWrite = has_flag(desc.usage, RHIBufferUsage::CPUWrite);
            auto hasCPURead = has_flag(desc.usage, RHIBufferUsage::CPURead);

            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage ||
                              hasCPUWrite || hasCPURead || desc.initialData != nullptr;

            auto isShaderResource = has_flag(desc.usage, RHIBufferUsage::ShaderResource);
            auto isUnorderedAccess = has_flag(desc.usage, RHIBufferUsage::UnorderedAccess);

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
            if(has_flag(desc.usage, RHIBufferUsage::StructuredBuffer))
                miscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            if(has_flag(desc.usage, RHIBufferUsage::IndirectArgs))
                miscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

            D3D11_BUFFER_DESC dxDesc = {
                .ByteWidth = static_cast<UINT>(desc.size),
                .Usage = isCPUAccessible ?
                    D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
                .BindFlags = bindFlags,
                .CPUAccessFlags = isCPUAccessible ?
                    D3D11_CPU_ACCESS_WRITE : UINT(0),
                .MiscFlags = miscFlags,
                .StructureByteStride = desc.stride
            };

            D3D11_SUBRESOURCE_DATA initData{
                .pSysMem = desc.initialData
            };

            if(FAILED(device->CreateBuffer(
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

            if(isShaderResource){
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
                    .Format = DXGI_FORMAT_R32_FLOAT,
                    .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
                    .Buffer = {
                        
                    }
                };
                device->CreateShaderResourceView(
                    buffer,
                    &srvDesc,
                    &srv
                );
            }
            if(isUnorderedAccess){
                D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{
                    .Format = DXGI_FORMAT_R32_FLOAT,
                    .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
                    .Buffer = {

                    }
                };
                device->CreateUnorderedAccessView(
                    buffer,
                    &uavDesc,
                    &uav
                );
            }
        }

        ~D3D11Buffer(){
            if(uav != nullptr){
                uav->Release();
                uav = nullptr;
            }
            if(srv != nullptr){
                srv->Release();
                srv = nullptr;
            }
            if(buffer != nullptr){
                buffer->Release();
                buffer = nullptr;
            }
        }

        CROWY_DECLARE_PINNED(D3D11Buffer)

        inline void upload(
            const void* data, size_t size,
            size_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(isCPUAccessible && size <= bufSize - offset);

            D3D11_MAPPED_SUBRESOURCE mapped;
            context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

            std::memcpy(mapped.pData, data, size);

            context->Unmap(buffer, 0);
        }

        inline void download(
            void* data, size_t size,
            size_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(isCPUAccessible && size <= bufSize - offset);
            D3D11_MAPPED_SUBRESOURCE mapped;
            context->Map(buffer, 0, D3D11_MAP_READ, 0, &mapped);

            std::memcpy(data, mapped.pData, size);

            context->Unmap(buffer, 0);
        }
        
        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            currentState = state;
        }

        ID3D11Buffer* get() const{ return buffer; }
        ID3D11ShaderResourceView* getSRV() const{ return srv; }
    };
}
