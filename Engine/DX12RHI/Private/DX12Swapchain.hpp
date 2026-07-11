#pragma once

#include <vector>
#include "RHIDefinitions.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Swapchain final{
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
            DescriptorHeapAllocator& dsvHeap
        );

        ~DX12Swapchain();

        bool AcquireNextImage() noexcept;

        void Resize(u32 newWidth, u32 newHeight);

        RHIPixelFormat GetFormat() const noexcept;
        u32 GetWidth() const noexcept;
        u32 GetHeight() const noexcept;

        RHITexture& GetCurrentTexture(){
            return *backBuffers[currentBackBufferIndex];
        }

        void Present();

    private:
        void createBackBuffers(u32 bufferCount);
    };
}
