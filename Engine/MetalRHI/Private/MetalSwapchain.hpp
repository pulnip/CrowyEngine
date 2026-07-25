#pragma once

#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLTexture.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include <SDL3/SDL_metal.h>
#include "MetalTexture.hpp"
#include "Primitives.hpp"
#include "RHIAPI.hpp"
// #include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    class MetalSwapchain final: public RHISwapchain{
    private:
        SDL_MetalView view;
        // Cache MetalLayer
        CA::MetalLayer* metalLayer = nullptr;
        CA::MetalDrawable* currentDrawable = nullptr;

        std::vector<RAII<MetalTexture>> backBuffers;
        const u64& frameIndex;

    public:
        MetalSwapchain(
            MTL::Device& device,
            const RHISwapchainCreateDesc& desc
        );

        ~MetalSwapchain();

        bool AcquireNextImage() noexcept RHI_OVERRIDE;

        void Resize(u32 newWidth, u32 newHeight) RHI_OVERRIDE;

        u32 GetWidth() const noexcept RHI_OVERRIDE{
            return backBuffers[currentIndex()]->GetWidth();
        }
        u32 GetHeight() const noexcept RHI_OVERRIDE{
            return backBuffers[currentIndex()]->GetHeight();
        }

        RHITexture& GetCurrentTexture() RHI_OVERRIDE{
            return *backBuffers[currentIndex()];
        }

        void Present() RHI_OVERRIDE;

        CA::MetalDrawable* GetCurrentDrawable() const noexcept{
            return currentDrawable;
        }

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % backBuffers.size()
            );
        }
    };
}
