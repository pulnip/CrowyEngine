#pragma once

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
        // frame N - 1 has ended here and frame N has not begun
        virtual void NewFrame() {}

        // false holds the loop between frames: the window stays alive, no
        // frame number is spent, and input that arrives meanwhile is dropped
        virtual bool ShouldAdvance() { return true; }

        // frame N has begun: the pacer assigned the number and waited for
        // frame N - FramesInFlight to complete
        virtual void OnFrameBegin(u64 frame) {}

        virtual void ProcessInput(const InputProvider&) {}
        virtual bool Update() { return true; };
        // asked after Update: true hides the cursor and keeps it in the
        // window, and the mouse reports motion that no window edge stops
        virtual bool WantsRelativeMouse() { return false; }
        virtual void Render(CommandListPool&, RHISwapchain& swapchain) = 0;

        // the frame is submitted; what it measured, pushed so the app never
        // has to reach into OS for it
        virtual void OnFrameEnd(const FrameReport&) {}

        virtual void Finalize() {}

        virtual void OnResize(u32 width, u32 height) {}
    };
}
