#pragma once

#include <array>
#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFrameStats.hpp"
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

    #if CROWY_BENCHMARK
        RHIFrameStats frameStats;
    #endif

    public:
        CommandListPool(RHIDevice&);
        ~CommandListPool();

        void BeginFrame();
        std::vector<RHICommandList*> ExtractRecorded();

        RHICommandList& Acquire();

    #if CROWY_BENCHMARK
        // valid once ExtractRecorded() has folded in this frame's lists
        const RHIFrameStats& GetFrameStats() const noexcept{ return frameStats; }
    #else
        const RHIFrameStats& GetFrameStats() const noexcept{
            static constexpr RHIFrameStats none;
            return none;
        }
    #endif

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % RHI_FRAMES_IN_FLIGHT
            );
        }
    };
}
