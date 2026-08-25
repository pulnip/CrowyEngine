#pragma once

#include <unordered_map>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHIBuffer.hpp"
#include "DX12Allocator.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Buffer: public RHIBuffer{
    private:
        RHIAllocation allocation{};
        // what the caller asked for, before any alignment padding
        u32 size = 0;
        void* mapped = nullptr;

        // descriptor heap index
        std::unordered_map<RHIBufferViewDesc, UINT> cbvs;
        std::unordered_map<RHIBufferViewDesc, UINT> srvs;
        std::unordered_map<RHIBufferViewDesc, UINT> uavs;

        DX12Allocator& allocator;
        // CBV, SRV, UAV
        DescriptorHeapAllocator& heap;

    public:
        DX12Buffer(
            DX12Allocator&,
            const RHIBufferCreateDesc&,
            DescriptorHeapAllocator&,
            StrView name
        );

        ~DX12Buffer();

        void Upload(
            const void* src,
            u32 srcSize,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void Download(
            void* dst,
            u32 dstSize,
            u32 offset = 0
        ) RHI_OVERRIDE;

        u32 GetSize() const noexcept RHI_OVERRIDE{ return size; }

        void* GetMappedPtr() noexcept RHI_OVERRIDE{ return mapped; }

        Buffer* Get() noexcept{ return allocation.resource; }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const noexcept{
            return allocation.resource->GetGPUVirtualAddress();
        }

        u64 GetReadableID(const RHIBufferViewDesc&) RHI_OVERRIDE;
        u64 GetWritableID(const RHIBufferViewDesc&) RHI_OVERRIDE;

    private:
        UINT createView(
            const RHIBufferViewDesc&,
            std::unordered_map<RHIBufferViewDesc, UINT>& cache,
            bool writable
        );
    };
}
