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
        const u64& frameIndex;

    public:
        CommandListPool(RHIDevice&);
        ~CommandListPool();

        void BeginFrame();
        void SubmitFrame(RHISwapchain* swapchain = nullptr);

        RHICommandList& Acquire();

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % RHI_FRAMES_IN_FLIGHT
            );
        }
    };
}
