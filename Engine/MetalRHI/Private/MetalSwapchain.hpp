#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
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
        MTL::Device* device = nullptr;
        CA::MetalLayer* metalLayer = nullptr;
        CA::MetalDrawable* currentDrawable = nullptr;

        uint32_t width = 0;
        uint32_t height = 0;
        RHITextureFormat format = RHITextureFormat::Unknown;

    public:
        MetalSwapchain(
            MTL::Device* device,
            const RHISwapchainCreateDesc& desc
        ){
            metalLayer = static_cast<CA::MetalLayer*>(desc.windowHandle);
            if(!metalLayer){
                throw std::runtime_error("Swapchain window handle is null");
            }

            metalLayer->setDevice(device);
            metalLayer->setPixelFormat(convertTextureFormat(desc.bufferDesc.format));
            metalLayer->setFramebufferOnly(true);
            metalLayer->setDrawableSize(CGSizeMake(desc.bufferDesc.width, desc.bufferDesc.height));
        }

        ~MetalSwapchain(){
            currentDrawable = nullptr;
        }

        bool acquireNextImage() RHI_OVERRIDE{
            currentDrawable = metalLayer->nextDrawable();
            return currentDrawable != nullptr;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) RHI_OVERRIDE{
            width = newWidth;
            height = newHeight;
            metalLayer->setDrawableSize(CGSizeMake(newWidth, newHeight));
            currentDrawable = nullptr;
        }

        MTL::Texture* getCurrentTexture() const{
            return currentDrawable ? currentDrawable->texture() : nullptr;
        }

        CA::MetalDrawable* getCurrentDrawable() const{ 
            return currentDrawable; 
        }

        void* getCurrentNativeTexture() const RHI_OVERRIDE{
            return getCurrentTexture();
        }
    };
}