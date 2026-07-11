#pragma once

#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLTexture.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include <SDL3/SDL_metal.h>
#include "MetalTexture.hpp"
#include "Primitives.hpp"
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"
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

        RAII<MetalTexture> backBuffer;

    public:
        MetalSwapchain(
            MTL::Device& device,
            const RHISwapchainCreateDesc& desc
        );

        ~MetalSwapchain();

        bool AcquireNextImage() noexcept RHI_OVERRIDE;

        void Resize(u32 newWidth, u32 newHeight) RHI_OVERRIDE;

        RHIPixelFormat GetFormat() const noexcept RHI_OVERRIDE{
            return backBuffer->GetFormat();
        }
        u32 GetWidth() const noexcept RHI_OVERRIDE{
            return backBuffer->GetWidth();
        }
        u32 GetHeight() const noexcept RHI_OVERRIDE{
            return backBuffer->GetHeight();
        }

        RHITexture& GetCurrentTexture() RHI_OVERRIDE{
            return *backBuffer;
        }
        const RHITexture& GetCurrentTexture() const RHI_OVERRIDE{
            return *backBuffer;
        }

        void Present(RHICommandList&) RHI_OVERRIDE;

        CA::MetalDrawable* GetCurrentDrawable() const noexcept{
            return currentDrawable;
        }
    };
}
