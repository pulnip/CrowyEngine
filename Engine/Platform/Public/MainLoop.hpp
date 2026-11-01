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

        void Init(RHIDevice& device, RHISwapchain& swapchain) {
            init(device, swapchain);
        }

        void NewFrame() { newFrame(); }

        void ProcessInput(const InputProvider& input) { processInput(input); }

        void Update(f64 deltaTime, f64 elapsedTime) {
            update(deltaTime, elapsedTime);
        }

        bool WantsRelativeMouse() { return wantsRelativeMouse(); }

        void Record(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) {
            record(cmdList, backBuffer);
        }

        void EndFrame(const FrameReport& report) { endFrame(report); }

        void Finalize() { finalize(); }

        void Resize(u32 width, u32 height) { resize(width, height); }

    private:
        virtual void init(RHIDevice&, RHISwapchain& swapchain) {}

        // the CPU-side frame boundary, before the frame reads any input:
        // frame N - 1 has ended here and frame N has not begun. A held loop
        // comes here too, between frames.
        virtual void newFrame() {}

        virtual void processInput(const InputProvider&) {}

        // the frame's time and the loop's total, with no hold in either
        virtual void update(f64 deltaTime, f64 elapsedTime) {}

        // asked after update: true hides the cursor and keeps it in the
        // window, and the mouse reports motion that no window edge stops
        virtual bool wantsRelativeMouse() { return false; }

        // inside the command list the loop opened and will close; the sample
        // owns its pass structure, so it owns the back buffer's barriers too
        // (AcquireBackBuffer / ReleaseBackBuffer)
        virtual void record(
            RHICommandList&,
            const RHIColorAttachment& backBuffer
        ) = 0;

        // the frame is submitted; what it measured, pushed so the app never
        // has to reach into OS for it
        virtual void endFrame(const FrameReport&) {}

        virtual void finalize() {}

        virtual void resize(u32 width, u32 height) {}
    };
}
