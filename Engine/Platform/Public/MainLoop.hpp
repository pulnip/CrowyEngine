#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class InputProvider;
    struct FrameReport;

    class MainLoop{
    public:
        CROWY_DECLARE_INTERFACE(MainLoop)

        virtual void OnInit(RHIDevice&, RHISwapchain& swapchain){}

        // the CPU-side frame boundary, before any input of the frame is
        // read: the drain point for whatever collected between frames.
        // frame N - 1 has ended here and frame N has not begun
        virtual void NewFrame(){}

        // between frames, right after NewFrame: false holds the loop there.
        // A held iteration keeps the window alive and drains again; nothing
        // that costs a frame number runs, and input that arrives meanwhile
        // is dropped
        virtual bool ShouldAdvance(){ return true; }

        // frame N has begun: the pacer assigned the number and waited for
        // frame N - RHI_FRAMES_IN_FLIGHT to complete
        virtual void OnFrameBegin(u64 frame){}

        virtual void ProcessInput(const InputProvider&){}
        virtual bool Update(){ return true; };
        // TODO. support multi-window if needed
        virtual void Render(CommandListPool&, RHISwapchain& swapchain) = 0;

        // the frame is submitted; what it measured, pushed so the app never
        // has to reach into OS for it
        virtual void OnFrameEnd(const FrameReport&){}

        virtual void Finalize(){}

        virtual void OnResize(u32 width, u32 height){}
    };
}
