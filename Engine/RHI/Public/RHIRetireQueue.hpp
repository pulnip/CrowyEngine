#pragma once

#include <deque>
#include <vector>

#include "Function.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // a reclaim deferred mid-frame has no batch to wait behind yet, so it waits
    // in `pending` until the next OnSubmit() attaches it to the next submission
    class RHIRetireQueue {
    public:
        using Reclaim = std::move_only_function<void()>;

    private:
        struct Entry {
            u64 tag;
            Reclaim reclaim;
        };

        std::vector<Reclaim> pending;
        std::deque<Entry> tagged;

    public:
        // call from anywhere mid-frame; the next OnSubmit() attaches this to
        // the batch about to be submitted
        void Defer(Reclaim reclaim);

        void OnSubmit(u64 value);

        void Collect(u64 completed);

        // reclaim everything unconditionally - only once nothing GPU-side
        // can still be reading any of it, e.g. device teardown
        void CollectAll();
    };
}
