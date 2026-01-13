#pragma once

#include "semantics.hpp"
#include "RHIDefinitions.hpp"
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
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHISwapchain)

        virtual bool acquireNextImage() noexcept = 0;

        virtual void resize(uint32_t newWidth, uint32_t newHeight) noexcept = 0;

        virtual void* getCurrentNativeTexture() const noexcept = 0;
    };
}
