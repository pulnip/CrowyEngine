#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class MainLoop{
    public:
        SMOL_DECLARE_INTERFACE(MainLoop)

        virtual bool Initialize(){ return true; }
        virtual bool Update() = 0;
        // TODO. support multi-window if needed
        virtual bool Render(CommandListPool&, RHISwapchain&) = 0;
        virtual bool RenderUI(RHICommandList&, RHISwapchain&) = 0;
        virtual void Finalize(){}
    };
}
