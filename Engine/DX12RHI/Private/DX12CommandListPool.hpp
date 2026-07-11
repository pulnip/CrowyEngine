#pragma once

#include <array>
#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class DX12Device;
    class DX12CommandList;

    class DX12CommandListPool{
    private:
        DX12Device& device;
        struct FrameSlot{
            std::vector<RAII<DX12CommandList>> cmdLists;
            usize nextIndex = 0;
        };
        std::array<FrameSlot, RHI_FRAMES_IN_FLIGHT> slots;
        const u64& frameIndex;

    public:
        DX12CommandListPool(DX12Device&);
        ~DX12CommandListPool();

        void BeginFrame();
        void SubmitFrame();

        DX12CommandList& Acquire();

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % RHI_FRAMES_IN_FLIGHT
            );
        }
    };
}
