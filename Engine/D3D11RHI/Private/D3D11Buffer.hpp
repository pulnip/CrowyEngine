#pragma once

#include <cstddef>
#include <memory>
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
        size_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;
        bool isCPUAccessible = false;
        RHIResourceState currentState = RHIResourceState::Common;
        ID3D11ShaderResourceView* srv = nullptr;

    public:
        D3D11Buffer(
            ID3D11Device* device,
            const RHIBufferCreateDesc& desc
        )
            : usage(desc.usage)
            , size(desc.size)
        {
            isCPUAccessible = hasFlag(desc.usage, RHIBufferUsage::CPUWrite);

            UINT bindFlags = 0;
            if(hasFlag(desc.usage, RHIBufferUsage::VertexBuffer))
                bindFlags |= D3D11_BIND_VERTEX_BUFFER;
            if(hasFlag(desc.usage, RHIBufferUsage::IndexBuffer))
                bindFlags |= D3D11_BIND_INDEX_BUFFER;
            if(hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer))
                bindFlags |= D3D11_BIND_CONSTANT_BUFFER;
            if(hasFlag(desc.usage, RHIBufferUsage::ShaderResource))
                bindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if(hasFlag(desc.usage, RHIBufferUsage::UnorderedAccess))
                bindFlags |= D3D11_BIND_UNORDERED_ACCESS;

            UINT miscFlags = 0;
            if(hasFlag(desc.usage, RHIBufferUsage::StructuredBuffer))
                miscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            if(hasFlag(desc.usage, RHIBufferUsage::IndirectArgs))
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
            if(!desc.debugName.empty()){
                buffer->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(desc.debugName.length()),
                    desc.debugName.c_str()
                );
            }
        #endif

            if(hasFlag(desc.usage, RHIBufferUsage::ShaderResource))
                device->CreateShaderResourceView(buffer, nullptr, &srv);
        }

        ~D3D11Buffer(){
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

        ID3D11Buffer* get() const{ return buffer; }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            currentState = state;
        }

        ID3D11ShaderResourceView* getSRV() const{ return srv; }
    };
}
