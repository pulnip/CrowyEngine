#pragma once

#include "semantics.hpp"
#include "RHIDefinitions.h"
#include "RHIFWD.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalSwapchain.hpp"
    #else
        #include "NullSwapchain.hpp"
    #endif
#endif

namespace Crowy
{
    // Swapchain for presenting rendered images to the screen
    class RHISwapchain{
    protected:
        uint32_t width;
        uint32_t height;
        uint32_t bufferCount;
        RHITextureFormat format;
        uint32_t currentBufferIndex;

    public:
        RHISwapchain(
            uint32_t width, uint32_t height,
            uint32_t bufferCount, RHITextureFormat format
        )
            :width(width), height(height)
            ,bufferCount(bufferCount), format(format)
            ,currentBufferIndex(0){}

        DECLARE_INTERFACE(RHISwapchain)

        inline uint32_t getWidth() const { return width; }
        inline uint32_t getHeight() const { return height; }
        inline uint32_t getBufferCount() const { return bufferCount; }
        inline RHITextureFormat getFormat() const { return format; }
        inline uint32_t getCurrentBufferIndex() const {
            return currentBufferIndex;
        }

        // Get current backbuffer for rendering
        virtual RHITexture* getCurrentBackbuffer() = 0;

        // Present rendered image to screen
        virtual void present(bool vsync = true) = 0;

        // Resize swapchain (window resize)
        virtual void resize(uint32_t newWidth, uint32_t newHeight) = 0;

        // Platform-specific swapchain getter
        virtual void* getNative() = 0;
    };
}
