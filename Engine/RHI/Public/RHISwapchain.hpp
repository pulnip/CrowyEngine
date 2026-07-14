#pragma once

#include "RHICommandList.hpp"
#include "Semantics.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Swapchain for presenting rendered images to the screen
    class RHISwapchain{
    public:
        CROWY_DECLARE_INTERFACE(RHISwapchain)

        virtual bool AcquireNextImage() = 0;

        virtual void Resize(u32 newWidth, u32 newHeight) = 0;

        virtual RHIPixelFormat GetFormat() const noexcept = 0;
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;

        virtual RHITexture& GetCurrentTexture() = 0;

        virtual void Present() = 0;
    };
}
