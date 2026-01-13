#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include "assert.hpp"
#include "MetalUtil.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif

namespace Crowy
{
    class MetalSwapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:
        CA::MetalLayer* metalLayer = nullptr;
        CA::MetalDrawable* currentDrawable = nullptr;

        uint32_t width = 0;
        uint32_t height = 0;
        RHITextureFormat format = RHITextureFormat::Unknown;

    public:
        MetalSwapchain(
            MTL::Device* device,
            const RHISwapchainCreateDesc& desc
        ) noexcept{
            metalLayer = static_cast<CA::MetalLayer*>(desc.windowHandle);
            CROWY_ASSERT(metalLayer != nullptr);

            metalLayer->setDevice(device);
            metalLayer->setPixelFormat(convertTextureFormat(desc.bufferDesc.format));
            metalLayer->setFramebufferOnly(true);
            metalLayer->setDrawableSize(CGSizeMake(desc.bufferDesc.width, desc.bufferDesc.height));
        }

        ~MetalSwapchain(){
            currentDrawable = nullptr;
        }

        bool acquireNextImage() noexcept RHI_OVERRIDE{
            currentDrawable = metalLayer->nextDrawable();
            return currentDrawable != nullptr;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) noexcept RHI_OVERRIDE{
            width = newWidth;
            height = newHeight;
            metalLayer->setDrawableSize(CGSizeMake(newWidth, newHeight));
            currentDrawable = nullptr;
        }

        MTL::Texture* getCurrentTexture() const noexcept{
            return currentDrawable ? currentDrawable->texture() : nullptr;
        }

        CA::MetalDrawable* getCurrentDrawable() const noexcept{ 
            return currentDrawable; 
        }

        void* getCurrentNativeTexture() const noexcept RHI_OVERRIDE{
            return getCurrentTexture();
        }
    };
}