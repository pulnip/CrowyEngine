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
    //
    // `heapOffset` is 0 for every committed resource. It exists so that a
    // heap-backed implementation can slot in underneath without reshaping
    // anything that holds one of these.
    struct RHIAllocation {
        Buffer* resource = nullptr;
        u64 heapOffset = 0;
        u64 size = 0;
    };

    // The single place that turns a resource description into memory.
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
        DX12Allocator(
            Device& device,
            const DX12Capabilities& capabilities
        );
        ~DX12Allocator();
        CROWY_DECLARE_PINNED(DX12Allocator)

        // GPU_UPLOAD is deliberately not a type of its own: whether Upload
        // can take it is a runtime capability, not something a caller asks for
        [[nodiscard]] RHIAllocation Allocate(
            const D3D12_RESOURCE_DESC1&,
            RHIMemoryType,
            const D3D12_CLEAR_VALUE* clearValue = nullptr,
            StrView name = {}
        );

        void Free(const RHIAllocation&);
    };
}
