#pragma once

#include <deque>
#include "Function.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Ring over one staging buffer, retired against the device's frame
    // timeline: an allocation stays readable until the batch it was submitted
    // with completes. The ring never sees a fence - the device hands it frame
    // values, the same split the rest of the RHI uses.
    class UploadRing{
    public:
        struct Allocation{
            RHIBuffer& buffer;
            u64 offset;
            // into the staging buffer's persistent mapping, already offset
            void* cpuPtr;
        };

        // Submits copies that are recorded but not yet handed to the queue.
        // Without it a ring filled entirely from one unsubmitted command list
        // has no frame value to wait on and would block forever.
        using Flush = std::move_only_function<void()>;

    private:
        struct InFlight{
            u64 head;
            u64 tag;
        };
        RHIDevice* device = nullptr;
        RHIBufferRAII staging;
        Flush flush;
        u64 capacity = 0;
        u64 head = 0, tail = 0;
        std::deque<InFlight> inFlight;

    #if defined(_DEBUG) || !defined(NDEBUG)
        // stamps freed bytes so a copy still reading a retired range shows up
        // in the destination instead of silently reproducing stale data
        bool poisonMode = false;
    #endif

    public:
        UploadRing() = default;
        // size should be mutiples of 512
        UploadRing(
            RHIDevice&,
            RHIBufferRAII stagingBuffer,
            Flush flush = {}
        );

        Allocation Allocate(
            u64 size,
            u64 align
        );

        // call from the device's submit path once the batch's frame value is
        // final: everything allocated since the last call rides that batch
        void OnSubmit(u64 tag);

    private:
        void retireCompleted();

    #if defined(_DEBUG) || !defined(NDEBUG)
        void poisonRange(u64 from, u64 to);
    #endif
    };
}
