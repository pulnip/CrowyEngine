#pragma once

#include <Metal/MTLDevice.hpp>
#include <Metal/MTLBuffer.hpp>
#include "MetalAllocator.hpp"
#include "RHIAPI.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class MetalBuffer final: public RHIBuffer{
    private:
        RHIAllocation allocation{};
        MTL::Buffer* buffer = nullptr;
        // what the caller asked for, before any alignment padding
        u32 size = 0;
        void* mapped = nullptr;

        MetalAllocator& allocator;

    public:
        MetalBuffer(
            MetalAllocator&,
            const RHIBufferCreateDesc&,
            StrView name = {}
        );
        ~MetalBuffer();

        void Upload(
            const void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void Download(
            void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE;

        u32 GetSize() const noexcept RHI_OVERRIDE{ return size; }

        void* GetMappedPtr() noexcept RHI_OVERRIDE{ return mapped; }

        u64 GetReadableID(const RHIBufferViewDesc& view) RHI_OVERRIDE{
            return getResourceID(view);
        }
        u64 GetWritableID(const RHIBufferViewDesc& view) RHI_OVERRIDE{
            return getResourceID(view);
        }

        MTL::Buffer* Get() noexcept{ return buffer; }

    private:
        u64 getResourceID(const RHIBufferViewDesc&);
    };
}
