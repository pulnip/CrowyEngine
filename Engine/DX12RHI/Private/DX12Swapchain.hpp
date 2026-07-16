#pragma once

#include <vector>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHISwapchain.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Swapchain: public RHISwapchain{
    private:
        SwapchainRAII swapchain = nullptr;
        bool vsync, allowTearing;

        DescriptorHeapAllocator& cbvsrvuavHeap;
        DescriptorHeapAllocator& rtvHeap;
        DescriptorHeapAllocator& dsvHeap;
        std::vector<RHITextureRAII> backBuffers;
        UINT currentBackBufferIndex = 0;

    public:
        DX12Swapchain(
            CommandQueue&,
            Factory&,
            const RHISwapchainCreateDesc&,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            StrView name = {}
        );

        ~DX12Swapchain();

        bool AcquireNextImage() noexcept RHI_OVERRIDE;

        void Resize(u32 newWidth, u32 newHeight) RHI_OVERRIDE;

        u32 GetWidth() const noexcept RHI_OVERRIDE;
        u32 GetHeight() const noexcept RHI_OVERRIDE;

        RHITexture& GetCurrentTexture() RHI_OVERRIDE{
            return *backBuffers[currentBackBufferIndex];
        }

        void Present() RHI_OVERRIDE;

    private:
        void createBackBuffers(u32 bufferCount);
    };
}
