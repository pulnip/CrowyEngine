#pragma once

#include <cstddef>
#include <memory>
#include <d3d11.h>
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
        size_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;
        bool isCPUAccessible = false;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        D3D11Buffer(
            ID3D11Device* device,
            const RHIBufferCreateDesc& desc
        )
            : usage(desc.usage)
            , size(desc.size)
        {
            auto hasVertexUsage = hasFlag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = hasFlag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer);
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
        }
        ~D3D11Buffer() = default;

        void update(const void* data, size_t size, size_t offset) noexcept RHI_OVERRIDE{

        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
