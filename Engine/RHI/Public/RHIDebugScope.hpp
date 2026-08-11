#pragma once

#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // Names a stretch of recorded commands for a GPU capture. PIX and
    // RenderDoc both show these, which is the difference between a capture
    // that reads as a frame and one that reads as three hundred draws.
    class RHIEventScope{
    private:
        RHICommandList& cmdList;

    public:
        RHIEventScope(RHICommandList& cmdList, CStr name)
            : cmdList(cmdList)
        {
            cmdList.BeginEvent(name);
        }
        ~RHIEventScope(){
            cmdList.EndEvent();
        }
        CROWY_DECLARE_PINNED(RHIEventScope)
    };
}
