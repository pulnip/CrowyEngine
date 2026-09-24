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
        virtual void RenderOnce(CommandListPool&){}

        // the CPU-side frame boundary, before any input of the frame is
        // read: the drain point for whatever collected between frames
        virtual void NewFrame(){}

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
