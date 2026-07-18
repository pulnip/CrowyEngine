#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class InputProvider;

    class MainLoop{
    public:
        CROWY_DECLARE_INTERFACE(MainLoop)

        virtual void OnInit(RHIDevice&){}

        virtual void ProcessInput(const InputProvider&){}
        virtual bool Update(){ return true; };
        // TODO. support multi-window if needed
        virtual bool Render(CommandListPool&, RHISwapchain&) = 0;

        virtual void Finalize(){}
    };
}
