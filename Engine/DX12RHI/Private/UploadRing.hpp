#pragma once

#include <deque>
#include "Primitives.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    class DX12Device;

    class UploadRing{
    public:
        struct Allocation{
            RHIBuffer& buffer;
            u64 offset;
        };

    private:
        struct InFlight{
            u64 head;
            RHIFence& fence;
            u64 value;
        };
        RHIBufferRAII staging;
        u64 capacity = 0;
        u64 head = 0, tail = 0;
        std::deque<InFlight> inFlight;

    public:
        UploadRing() = default;
        // size should be mutiples of 512
        UploadRing(RHIBufferRAII stagingBuffer);

        Allocation Allocate(
            u64 size,
            u64 align
        );

        void OnSubmit(
            RHIFence& fence,
            u64 value
        );

    private:
        void retireOldest(bool wait);
    };
}
