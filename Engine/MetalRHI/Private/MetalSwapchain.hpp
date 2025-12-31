#pragma once

#include <Metal/Metal.hpp>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
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
    public:
        MetalSwapchain(
            MTL::Device* device,
            const RHISwapchainCreateDesc& desc
        ){}

        RHITexture* getCurrentBackbuffer() RHI_OVERRIDE{

        }

        void present(bool vsync = true) RHI_OVERRIDE{

        }

        void resize(uint32_t newWidth, uint32_t newHeight) RHI_OVERRIDE{

        }

        void* getNative() RHI_OVERRIDE{
            return nullptr;
        }
    };
}