#pragma once

#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // PIX and RenderDoc show these, so a capture reads as a frame rather than
    // as three hundred draws
    class RHIEventScope {
    private:
        RHICommandList& cmdList;

    public:
        RHIEventScope(RHICommandList& cmdList, CStr name)
            : cmdList(cmdList) {
            cmdList.BeginEvent(name);
        }
        ~RHIEventScope() { cmdList.EndEvent(); }
        CROWY_DECLARE_PINNED(RHIEventScope)
    };
}
