#pragma once

#include <unordered_map>

#include "DX12Allocator.hpp"
#include "DX12Definitions.hpp"
#include "RHIAPI.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class DX12Buffer: public RHIBuffer {
    private:
        DX12Allocation allocation{};
        // what the caller asked for, before any alignment padding
        u32 size = 0;
        void* mapped = nullptr;

        std::unordered_map<RHIBufferViewDesc, UINT> srvs;
        std::unordered_map<RHIBufferViewDesc, UINT> uavs;

        DX12Allocator& allocator;
        // CBV, SRV, UAV
        DX12DescriptorHeapAllocator& heap;

    public:
        DX12Buffer(
            DX12Allocator&,
            const RHIBufferCreateDesc&,
            DX12DescriptorHeapAllocator&,
            StrView name
        );

        ~DX12Buffer();

        u32 GetSize() const noexcept RHI_OVERRIDE { return size; }

        void* GetMappedPtr() noexcept RHI_OVERRIDE { return mapped; }

        Buffer* Get() noexcept { return allocation.resource; }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const noexcept {
            return allocation.resource->GetGPUVirtualAddress();
        }

    private:
        u64 readableIDOf(const RHIBufferViewDesc&) RHI_OVERRIDE;
        u64 writableIDOf(const RHIBufferViewDesc&) RHI_OVERRIDE;

        UINT createView(
            const RHIBufferViewDesc&,
            std::unordered_map<RHIBufferViewDesc, UINT>& cache,
            bool writable
        );
    };
}
