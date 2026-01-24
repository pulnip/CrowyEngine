#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif

namespace Crowy
{
    class D3D11Swapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    public:
        D3D11Swapchain(
            const RHISwapchainCreateDesc& desc
        ){}

        bool acquireNextImage() noexcept RHI_OVERRIDE{
            return true;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) noexcept RHI_OVERRIDE{

        }

        void* getCurrentNativeTexture() const noexcept RHI_OVERRIDE{
            return nullptr;
        }
    };
}