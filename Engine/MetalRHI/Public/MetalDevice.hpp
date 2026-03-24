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
        MetalDevice() noexcept;
        ~MetalDevice();

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHITexturePtr createTexture(
            const RHITextureCreateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHIShaderPtr createShader(
            const RHIShaderCreateDesc&
        ) RHI_OVERRIDE;
        RHISamplerPtr createSampler(
            const RHISamplerState&
        ) noexcept RHI_OVERRIDE;

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc&
        ) noexcept RHI_OVERRIDE;

        RHICommandListPtr createCommandList() noexcept RHI_OVERRIDE;

        RHIFencePtr createFence(uint64_t initialValue = 0) noexcept RHI_OVERRIDE;

        RHICapabilities getCapabilities() const noexcept RHI_OVERRIDE;

        void submit(RHICommandList&, RHISwapchain*) noexcept RHI_OVERRIDE;

        void* getNative() noexcept RHI_OVERRIDE;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
