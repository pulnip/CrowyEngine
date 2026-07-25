#pragma once

#include "Semantics.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // Swapchain for presenting rendered images to the screen
    class RHISwapchain{
    private:
        // Requested format; Could be differ from Actual format
        RHIPixelFormat format;

    public:
        RHISwapchain(
            RHIPixelFormat format
        )
            : format(format)
        {}
        virtual ~RHISwapchain() = default;
        CROWY_DECLARE_PINNED(RHISwapchain)

        virtual bool AcquireNextImage() = 0;

        virtual void Resize(u32 newWidth, u32 newHeight) = 0;

        RHIPixelFormat GetFormat() const noexcept{
            return format;
        }
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;

        virtual RHITexture& GetCurrentTexture() = 0;
    };
}
