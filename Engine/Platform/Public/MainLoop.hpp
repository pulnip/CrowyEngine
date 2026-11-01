#pragma once

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class InputProvider;
    struct FrameReport;

    class MainLoop {
    public:
        CROWY_DECLARE_INTERFACE(MainLoop)

        virtual void OnInit(RHIDevice&, RHISwapchain& swapchain) {}

        // the CPU-side frame boundary, before the frame reads any input:
        // frame N - 1 has ended here and frame N has not begun. A held loop
        // comes here too, between frames.
        virtual void NewFrame() {}

        virtual void ProcessInput(const InputProvider&) {}

        // the frame's time and the loop's total, with no hold in either
        virtual void OnUpdate(f64 deltaTime, f64 elapsedTime) {}

        // asked after OnUpdate: true hides the cursor and keeps it in the
        // window, and the mouse reports motion that no window edge stops
        virtual bool WantsRelativeMouse() { return false; }

        // inside the command list the loop opened and will close; the sample
        // owns its pass structure, so it owns the back buffer's barriers too
        // (AcquireBackBuffer / ReleaseBackBuffer)
        virtual void OnRecord(
            RHICommandList&,
            const RHIColorAttachment& backBuffer
        ) = 0;

        // the frame is submitted; what it measured, pushed so the app never
        // has to reach into OS for it
        virtual void OnFrameEnd(const FrameReport&) {}

        virtual void Finalize() {}

        virtual void OnResize(u32 width, u32 height) {}
    };
}
