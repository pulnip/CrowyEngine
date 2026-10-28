#pragma once

#include <functional>
#include <span>

#include "CommandPort.hpp"
#include "FrameGraph.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "TargetCapture.hpp"

namespace Crowy
{
    // Every capture a frame owes, by its two routes: the back buffer through
    // the swapchain's dump, a named target through the walker's readback.
    class FrameCaptures {
    public:
        using LastFrame = std::function<u64()>;

    private:
        RHIDevice& device;
        RHISwapchain& swapchain;
        // the owner's, which a rebuild replaces
        const RAII<FrameGraph>& pipeline;
        LastFrame lastFrame;
        TargetCaptureQueue targets;
        // this frame's, filled by the walker and then handed to the queue
        TargetReadbacks due;
        // captures whose dump failed, since launch
        u32 failures = 0;

    public:
        ~FrameCaptures() = default;
        CROWY_DECLARE_PINNED(FrameCaptures)

        FrameCaptures(
            RHIDevice& device,
            RHISwapchain& swapchain,
            const RAII<FrameGraph>& pipeline,
            LastFrame lastFrame
        );

        // capture_frame
        void RegisterVerbs(CommandPort& port);

        // the back buffer of `frame` to `path` as a BMP, with the
        // capture_frame verb's checks; why not, empty once queued
        Str Request(Str path, u64 frame);
        // the requests due this frame, one per target, resolved by name
        std::span<TargetReadback> TakeDue();
        // after the walker recorded what TakeDue gave it
        void HandOff();
        void Collect();

        // both routes
        usize Pending() const noexcept;
        u32 Failures() const noexcept { return failures; }

    private:
        // why the running pipeline's target `name` cannot be captured; empty
        // when it can
        Str refuseTarget(StrView name) const;
        // a path taken in either queue, or a frame taken in the one asked
        // about: the swapchain's for no target, the target's otherwise
        bool isQueued(u64 frame, StrView target, StrView path) const;
        // why `frames` cannot be captured to `paths`, the back buffer for an
        // empty target; empty when every one can
        Str refuse(
            StrView target,
            std::span<const u64> frames,
            std::span<const Str> paths
        ) const;
        void queue(
            StrView target,
            std::span<const u64> frames,
            std::span<const Str> paths
        );
    };
}
