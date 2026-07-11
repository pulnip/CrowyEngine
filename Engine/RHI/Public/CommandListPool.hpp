#pragma once

#include <array>
#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    class CommandListPool{
    private:
        RHIDevice& device;
        struct FrameSlot{
            std::vector<RHICommandListRAII> cmdLists;
            usize nextIndex = 0;
        };
        std::array<FrameSlot, RHI_FRAMES_IN_FLIGHT> slots;
        u32 frameIndex = 0;

    public:
        CommandListPool(RHIDevice&);
        ~CommandListPool();

        RHICommandList& Acquire();

    private:
        friend class OS;

        void BeginFrame();
        void SubmitFrame(RHIFence&, u64 fenceValue);
    };
}
