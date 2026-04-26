#pragma once

#include <cstdint>
#include <memory>
#include "semantics.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalSwapchain.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11Swapchain.hpp"
    #else
        #include "NullSwapchain.hpp"
    #endif
#endif

namespace Crowy
{
    // Swapchain for presenting rendered images to the screen
    class RHISwapchain{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHISwapchain)

        virtual bool acquireNextImage() noexcept = 0;

        virtual void resize(uint32_t newWidth, uint32_t newHeight) noexcept = 0;

        virtual RHIPixelFormat getFormat() const noexcept = 0;
        virtual uint32_t getWidth() const noexcept = 0;
        virtual uint32_t getHeight() const noexcept = 0;

        virtual void* getCurrentNativeTexture() const noexcept = 0;
    };

    using RHISwapchainPtr = std::unique_ptr<RHISwapchain>;
}
