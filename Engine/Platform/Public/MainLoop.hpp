#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class InputProvider;

    class MainLoop{
    public:
        CROWY_DECLARE_INTERFACE(MainLoop)

        virtual void OnInit(RHIDevice&, RHISwapchain& swapchain){}
        virtual void RenderOnce(CommandListPool&){}

        virtual void ProcessInput(const InputProvider&){}
        virtual bool Update(){ return true; };
        // TODO. support multi-window if needed
        virtual void Render(CommandListPool&, RHISwapchain& swapchain) = 0;

        virtual void Finalize(){}

        virtual void OnResize(u32 width, u32 height){}
    };
}
