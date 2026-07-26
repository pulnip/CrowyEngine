#pragma once

#include "RHIDefinitions.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Sampler{
    private:
        UINT heapIndex;
        DescriptorHeapAllocator& samplerHeap;

    public:
        DX12Sampler(
            const RHISamplerState&,
            DescriptorHeapAllocator& samplerHeap
        );

        ~DX12Sampler();

        UINT Get() const noexcept{ return heapIndex; }
    };
}
