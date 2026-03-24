#pragma once

#include <cstddef>
#include "assert.hpp"
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
        const std::string debugName;

    public:
        NullBuffer(const RHIBufferCreateDesc& desc, const std::string& name)
            : usage(desc.usage)
            , size(desc.size)
            , debugName(name)
        {
            auto hasVertexUsage = has_flag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = has_flag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = has_flag(desc.usage, RHIBufferUsage::ConstantBuffer);
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
        }
        ~NullBuffer() = default;

        inline void upload(
            const void* data, size_t size,
            size_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(size <= bufSize - offset);
        }

        inline void download(
            void* data, size_t size,
            size_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(size <= bufSize - offset);
        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
