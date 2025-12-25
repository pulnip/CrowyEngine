#pragma once

#include <cstddef>
#include <memory>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
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
    public:
        NullBuffer(const RHIBufferCreateDesc& desc)
            : usage(desc.usage)
            , size(desc.size)
        {
            auto hasVertexUsage = (desc.usage & BUF_VertexBuffer) != 0;
            auto hasIndexUsage = (desc.usage & BUF_IndexBuffer) != 0;
            auto hasConstantUsage = (desc.usage & BUF_ConstantBuffer) != 0;
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
        }
        ~NullBuffer() = default;

    private:
        size_t size = 0;
        RHIBufferUsageFlags usage = RHIBufferUsageFlags::BUF_None;
        bool isCPUAccessible = false;
    };
}
