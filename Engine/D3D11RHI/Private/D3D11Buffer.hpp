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
        RHIResourceState currentState = RHIResourceState::Common;
        // ID3D11ShaderResourceView* srv = nullptr;
        // ID3D11UnorderedAccessView* uav = nullptr;

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

            // if(isShaderResource){
            //     D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
            //         .Format = DXGI_FORMAT_R32_FLOAT,
            //         .ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
            //         .Buffer = {
                        
            //         }
            //     };
            //     device->CreateShaderResourceView(
            //         buffer,
            //         &srvDesc,
            //         &srv
            //     );
            // }
            // if(isUnorderedAccess){
            //     D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{
            //         .Format = DXGI_FORMAT_R32_FLOAT,
            //         .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
            //         .Buffer = {

            //         }
            //     };
            //     device->CreateUnorderedAccessView(
            //         buffer,
            //         &uavDesc,
            //         &uav
            //     );
            // }
        }

        ~D3D11Buffer(){
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
            CROWY_ASSERT(size <= bufSize - offset);

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
            CROWY_ASSERT(size <= bufSize - offset);
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
    };
}
