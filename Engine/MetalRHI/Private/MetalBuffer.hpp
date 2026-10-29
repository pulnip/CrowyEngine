#pragma once

#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLDevice.hpp>

#include "MetalAllocator.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class MetalBuffer final: public RHIBuffer {
    private:
        MetalAllocation allocation{};
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

        u32 GetSize() const noexcept override { return size; }

        void* GetMappedPtr() noexcept override { return mapped; }

        MTL::Buffer* Get() noexcept { return buffer; }

    private:
        u64 readableIDOf(const RHIBufferViewDesc& view) override {
            return getResourceID(view);
        }
        u64 writableIDOf(const RHIBufferViewDesc& view) override {
            return getResourceID(view);
        }

        u64 getResourceID(const RHIBufferViewDesc&);
    };
}
