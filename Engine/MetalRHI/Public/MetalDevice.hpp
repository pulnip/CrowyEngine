#pragma once

#include <memory>
#include "RHIAPI.hpp"
#ifdef USE_STATIC_RHI
    #include "RHIDefinitions.hpp"
#else
    #include "RHIDevice.hpp"
#endif

namespace Crowy
{
    class MetalDevice
#ifndef USE_STATIC_RHI
        : public RHIDevice
#endif
    {
    public:
        MetalDevice();
        ~MetalDevice();

        RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) RHI_OVERRIDE;
        RHITexturePtr createTexture(const RHITextureCreateDesc&) RHI_OVERRIDE;
        RHIShaderPtr  createShader (const RHIShaderCreateDesc& ) RHI_OVERRIDE;

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc&
        ) RHI_OVERRIDE;
        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc&
        ) RHI_OVERRIDE;

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc&
        ) RHI_OVERRIDE;

        RHICommandListPtr createCommandList() RHI_OVERRIDE;

        RHIFencePtr createFence(uint64_t initialValue = 0) RHI_OVERRIDE;

        RHICapabilities getCapabilities() const RHI_OVERRIDE;

        void submit(RHICommandList*, RHISwapchain*) RHI_OVERRIDE;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
