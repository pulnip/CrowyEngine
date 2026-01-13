#pragma once

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
    public:
        NullSwapchain(
            const RHISwapchainCreateDesc& desc
        ){}

        bool acquireNextImage() RHI_OVERRIDE{
            return true;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) RHI_OVERRIDE{

        }

        void* getCurrentNativeTexture() const RHI_OVERRIDE{
            return nullptr;
        }
    };
}