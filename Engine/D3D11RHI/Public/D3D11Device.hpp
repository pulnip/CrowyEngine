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
    class D3D11Device
#ifndef USE_STATIC_RHI
        : public RHIDevice
#endif
    {
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

    public:
        D3D11Device();
        ~D3D11Device();

        RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) noexcept RHI_OVERRIDE;
        RHITexturePtr createTexture(const RHITextureCreateDesc&) noexcept RHI_OVERRIDE;
        RHIShaderPtr  createShader (const RHIShaderCreateDesc& ) noexcept RHI_OVERRIDE;
        RHISamplerPtr createSampler(const RHISamplerState&) noexcept RHI_OVERRIDE;

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc&
        ) noexcept RHI_OVERRIDE;
        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc&
        ) noexcept RHI_OVERRIDE;

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc&
        ) noexcept RHI_OVERRIDE; 

        RHICommandListPtr createCommandList() noexcept RHI_OVERRIDE;

        RHIFencePtr createFence(uint64_t initialValue = 0) noexcept RHI_OVERRIDE;

        RHICapabilities getCapabilities() const noexcept RHI_OVERRIDE;

        void submit(RHICommandList&, RHISwapchain&) noexcept RHI_OVERRIDE;

        void* getNative() noexcept RHI_OVERRIDE;
    };
}
