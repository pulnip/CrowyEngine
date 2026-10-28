#pragma once

#include <Metal/MTLCommandQueue.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include <QuartzCore/CAMetalLayer.hpp>
#include <SDL3/SDL_metal.h>

#include "MetalTexture.hpp"
#include "Primitives.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    class MetalCommandList;

    class MetalSwapchain final: public RHISwapchain {
    private:
        SDL_MetalView view = nullptr;
        CA::MetalLayer* metalLayer = nullptr;
        CA::MetalDrawable* currentDrawable = nullptr;

        MetalTexture backBuffer;

    public:
        MetalSwapchain(MTL::Device&, const RHISwapchainCreateDesc&);

        ~MetalSwapchain();

        bool AcquireNextImage() noexcept override;

        void Resize(u32 newWidth, u32 newHeight) override;

        // read from the layer, not the backbuffer;
        // the backbuffer is empty until the first AcquireNextImage.
        u32 GetWidth() const noexcept override {
            return static_cast<u32>(metalLayer->drawableSize().width);
        }
        u32 GetHeight() const noexcept override {
            return static_cast<u32>(metalLayer->drawableSize().height);
        }

        RHITexture& GetCurrentTexture() override { return backBuffer; }

        // `frame` is the loop frame this present belongs to
        void Present(MTL::CommandBuffer&, u64 frame);
    };
}
