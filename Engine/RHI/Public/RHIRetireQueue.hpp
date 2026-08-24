#pragma once

#include <deque>
#include <functional>
#include <vector>
#include "Primitives.hpp"

namespace Crowy
{
    // Retires GPU-referenced state once nothing GPU-side can still read it.
    // A reclaim deferred mid-frame has no batch to wait behind yet - the
    // frame's commands are still being recorded - so it waits in `pending`
    // until the next Tag() attaches it to whatever is about to be submitted.
    class RHIRetireQueue{
    public:
        using Reclaim = std::move_only_function<void()>;

    private:
        struct Entry{
            u64 tag;
            Reclaim reclaim;
        };

        std::vector<Reclaim> pending;
        std::deque<Entry> tagged;

    public:
        // call from anywhere mid-frame; the next Tag() attaches this to
        // the batch about to be submitted
        void Defer(Reclaim reclaim);

        // Submit (a): stamp everything deferred since the last Tag with
        // this batch's frame value
        void Tag(u64 value);

        // Submit entry (b): reclaim everything whose batch has completed
        void Collect(u64 completed);

        // reclaim everything unconditionally - only once nothing GPU-side
        // can still be reading any of it, e.g. device teardown
        void CollectAll();
    };
}
