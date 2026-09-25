#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include "Semantics.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    inline constexpr u32 MaxFrameDumps = 64;

    enum class FrameDumpState: u8{
        Pending,
        Written,
        Failed
    };

    // what became of one frame dump request
    struct FrameDumpOutcome{
        // the frame asked for, and the one whose image was written: a later
        // one when the frame asked for presented nothing
        u64 requested = 0;
        u64 presented = 0;
        Str path;
        bool written = false;
    };

    // Swapchain for presenting rendered images to the screen
    class RHISwapchain{
    protected:
        // the backend's half of one request: write the presented image to
        // path, then report through completion, from any thread
        struct FrameDumpJob{
            Str path;
            std::function<void(bool written)> completion;
        };

    private:
        using FrameDumpSignal = std::shared_ptr<std::atomic<FrameDumpState>>;

        struct FrameDumpRequest{
            u64 frame = 0;
            Str path;
        };

        struct FrameDumpInFlight{
            FrameDumpOutcome outcome;
            // completion may arrive from another thread (Metal)
            FrameDumpSignal state;
        };

        // Requested format; Could be differ from Actual format
        RHIPixelFormat format;

        // in frame order, one per frame. CROWY_DUMP_FRAME / _AT seed one at
        // construction
        std::vector<FrameDumpRequest> dumpRequests;
        // taken by the backend, until TakeFrameDumpOutcomes hands them over
        std::vector<FrameDumpInFlight> dumpsInFlight;

    public:
        RHISwapchain(RHIPixelFormat format);
        // joins the dumps still being written; logs every request that
        // never ran
        virtual ~RHISwapchain();
        CROWY_DECLARE_PINNED(RHISwapchain)

        virtual bool AcquireNextImage() = 0;

        virtual void Resize(u32 newWidth, u32 newHeight) = 0;

        RHIPixelFormat GetFormat() const noexcept{
            return format;
        }
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;

        virtual RHITexture& GetCurrentTexture() = 0;

        // whether that frame or that path already has a dump queued or
        // being written
        bool IsFrameDumpQueued(u64 frame, StrView path) const noexcept;
        // dump `frame` to path as a BMP once it presents. The caller checks
        // IsFrameDumpQueued and the MaxFrameDumps cap first
        void RequestFrameDump(Str path, u64 frame);
        // queued, plus finished but not yet taken: 0 only once every
        // outcome is in hand
        u32 PendingFrameDumps() const noexcept;
        // the dumps that finished since the last call, oldest first
        std::vector<FrameDumpOutcome> TakeFrameDumpOutcomes();

    protected:
        // the backend's half, called once per presented frame until it
        // comes back empty: every request due at or before `frame`. A frame
        // without a drawable presents nothing, so its request rides on the
        // next one, and the outcome says which
        std::optional<FrameDumpJob> TakeFrameDump(u64 frame);
    };
}
