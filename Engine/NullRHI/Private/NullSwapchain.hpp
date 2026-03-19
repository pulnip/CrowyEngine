#pragma once

#include <cstdint>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif

namespace Crowy
{
    class NullSwapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:
        uint32_t width = 0;
        uint32_t height = 0;

    public:
        NullSwapchain(
            const RHISwapchainCreateDesc& desc
        ) noexcept
            : width(desc.bufferDesc.width)
            , height(desc.bufferDesc.height)
        {}

        bool acquireNextImage() noexcept RHI_OVERRIDE{
            return true;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) noexcept RHI_OVERRIDE{

        }

        uint32_t getWidth() const noexcept RHI_OVERRIDE{
            return width;
        }
        uint32_t getHeight() const noexcept RHI_OVERRIDE{
            return height;
        }

        void* getCurrentNativeTexture() const noexcept RHI_OVERRIDE{
            return nullptr;
        }
    };
}