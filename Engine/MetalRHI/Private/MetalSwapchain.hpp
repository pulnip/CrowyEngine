#pragma once

#include <Metal/MTLCommandQueue.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include <SDL3/SDL_metal.h>
#include "MetalTexture.hpp"
#include "Primitives.hpp"
#include "RHIAPI.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    class MetalCommandList;

    class MetalSwapchain final: public RHISwapchain{
    private:
        SDL_MetalView view = nullptr;
        // Cache MetalLayer
        CA::MetalLayer* metalLayer = nullptr;
        CA::MetalDrawable* currentDrawable = nullptr;

        RAII<MetalTexture> backBuffer;

    public:
        MetalSwapchain(
            MTL::Device&,
            const RHISwapchainCreateDesc&
        );

        ~MetalSwapchain();

        bool AcquireNextImage() noexcept RHI_OVERRIDE;

        void Resize(u32 newWidth, u32 newHeight) RHI_OVERRIDE;

        u32 GetWidth() const noexcept RHI_OVERRIDE{
            return backBuffer->GetWidth();
        }
        u32 GetHeight() const noexcept RHI_OVERRIDE{
            return backBuffer->GetHeight();
        }

        RHITexture& GetCurrentTexture() RHI_OVERRIDE{
            return *backBuffer;
        }

        void Present(MetalCommandList&);
    };
}
