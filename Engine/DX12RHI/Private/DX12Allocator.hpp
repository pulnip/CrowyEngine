#pragma once

#include <unordered_map>

#include "DX12Definitions.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // One allocation, borrowed. DX12Allocator owns the resource until Free,
    // so this stays a plain value that can be copied around freely.
    struct DX12Allocation {
        Buffer* resource = nullptr;
    };

    // The single place that turns an RHI buffer or texture into memory.
    // Nothing outside this class calls CreateCommittedResource3, which is
    // what lets the strategy change without touching a caller.
    class DX12Allocator {
    private:
        Device& device;
        const DX12Capabilities& capabilities;
        // committed resources carry no block table of their own, so this
        // stands in for one - and it is the seam a heap-backed
        // implementation replaces
        std::unordered_map<Buffer*, COMRAII<Buffer>> live;

    public:
        DX12Allocator(Device& device, const DX12Capabilities& capabilities);
        ~DX12Allocator();
        CROWY_DECLARE_PINNED(DX12Allocator)

        // GPU_UPLOAD is deliberately not a type of its own: whether Upload
        // can take it is a runtime capability, not something a caller asks for
        [[nodiscard]] DX12Allocation Allocate(
            const D3D12_RESOURCE_DESC1&,
            RHIMemoryType,
            const D3D12_CLEAR_VALUE* clearValue = nullptr,
            StrView name = {}
        );

        void Free(const DX12Allocation&);
    };
}
