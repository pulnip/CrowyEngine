#pragma once

#include <cstddef>
#include <memory>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBuffer.hpp"
#endif

namespace Crowy
{
    class NullBuffer
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
        NullBuffer(const RHIBufferCreateDesc& desc)
            : usage(desc.usage)
            , size(desc.size)
        {
            auto hasVertexUsage = hasFlag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = hasFlag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer);
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
        }
        ~NullBuffer() = default;

        inline void upload(
            const void* data, size_t size,
            size_t offset = 0
        ) noexcept RHI_OVERRIDE{

        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
