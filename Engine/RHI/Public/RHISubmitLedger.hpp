#pragma once

#include "Primitives.hpp"
#include "RHIFence.hpp"
#include "RHIFrameTimeline.hpp"
#include "RHIRetireQueue.hpp"
#include "UploadRing.hpp"

namespace Crowy
{
    // what every submission records, in order; the pieces stay in the device,
    // each declared where its lifetime needs it
    class RHISubmitLedger {
    private:
        RHIFence& serialFence;
        RHIRetireQueue& retireQueue;
        UploadRing& uploadRing;
        UploadRing& transientRing;
        RHIFrameTimeline& timeline;

    public:
        RHISubmitLedger(
            RHIFence& serialFence,
            RHIRetireQueue& retireQueue,
            UploadRing& uploadRing,
            UploadRing& transientRing,
            RHIFrameTimeline& timeline
        )
            : serialFence(serialFence),
              retireQueue(retireQueue),
              uploadRing(uploadRing),
              transientRing(transientRing),
              timeline(timeline) {}

        // the serial completed as of entry, before this batch's own tag
        // exists; what it covers is reclaimed
        u64 Collect() {
            const auto completed = serialFence.GetValue();
            retireQueue.Collect(completed);

            return completed;
        }

        // everything deferred or allocated since the last batch rides `serial`
        void Stamp(u64 frame, u64 serial, u64 completed) {
            retireQueue.OnSubmit(serial);
            uploadRing.OnSubmit(serial);
            transientRing.OnSubmit(serial);
            timeline.OnSubmit(frame, serial, completed);
        }
    };
}
