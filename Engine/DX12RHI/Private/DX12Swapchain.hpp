#pragma once

#include <vector>

#include "DX12Definitions.hpp"
#include "RHIDefinitions.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    class DX12Swapchain: public RHISwapchain {
    private:
        RefPtr<Swapchain> swapchain = nullptr;
        // owned by DX12Device, which outlives the swapchain;
        // kept for the frame dump readback copy
        CommandQueue* queue = nullptr;
        bool vsync, allowTearing;

        DX12DescriptorHeapAllocator& cbvsrvuavHeap;
        DX12DescriptorHeapAllocator& rtvHeap;
        DX12DescriptorHeapAllocator& dsvHeap;
        std::vector<RHITextureRAII> backBuffers;
        UINT currentBackBufferIndex = 0;

    public:
        DX12Swapchain(
            CommandQueue&,
            Factory&,
            const RHISwapchainCreateDesc&,
            DX12DescriptorHeapAllocator& cbvsrvuavHeap,
            DX12DescriptorHeapAllocator& rtvHeap,
            DX12DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );

        ~DX12Swapchain();

        bool AcquireNextImage() noexcept override;

        void Resize(u32 newWidth, u32 newHeight) override;

        u32 GetWidth() const noexcept override;
        u32 GetHeight() const noexcept override;

        RHITexture& GetCurrentTexture() override {
            return *backBuffers[currentBackBufferIndex];
        }

        // `frame` is the loop frame this present belongs to
        void Present(u64 frame);

    private:
        void createBackBuffers(u32 bufferCount);
    };
}
