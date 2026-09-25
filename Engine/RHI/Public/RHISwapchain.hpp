#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include "Semantics.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    enum class FrameDumpState: u8{
        Idle,
        Pending,
        Written,
        Failed
    };

    // Swapchain for presenting rendered images to the screen
    class RHISwapchain{
    private:
        using FrameDumpSignal = std::shared_ptr<std::atomic<FrameDumpState>>;

        // Requested format; Could be differ from Actual format
        RHIPixelFormat format;

        // one frame dump request at a time: a path, and the loop frame it is
        // for (0 = the next one presented). CROWY_DUMP_FRAME /
        // CROWY_DUMP_FRAME_AT seed it once at construction, RequestFrameDump
        // sets it at runtime
        Str dumpPath;
        u64 dumpAtFrame = 0;
        // completion may arrive from another thread (Metal), so the backend
        // gets a handle it can hand to a completion handler
        FrameDumpSignal dumpState = std::make_shared<std::atomic<FrameDumpState>>(FrameDumpState::Idle);

    public:
        RHISwapchain(RHIPixelFormat format);
        virtual ~RHISwapchain() = default;
        CROWY_DECLARE_PINNED(RHISwapchain)

        virtual bool AcquireNextImage() = 0;

        virtual void Resize(u32 newWidth, u32 newHeight) = 0;

        RHIPixelFormat GetFormat() const noexcept{
            return format;
        }
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;

        virtual RHITexture& GetCurrentTexture() = 0;

        // dump the next presented frame to path as a BMP; false while an
        // earlier request is still pending. Poll GetFrameDumpState for the
        // outcome, which is written only once the file is on disk
        bool RequestFrameDump(Str path);
        FrameDumpState GetFrameDumpState() const noexcept{
            return dumpState->load(std::memory_order_acquire);
        }

    protected:
        // the backend's half, called once per presented frame: hands back
        // the pending request on the first presented frame at or after the
        // one it asked for
        std::optional<Str> TakeFrameDump(u64 frame) noexcept;
        // what the backend calls with the write result, from any thread
        std::function<void(bool written)> FrameDumpCompletion() const;
    };
}
