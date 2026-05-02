#pragma once

#include <memory>
#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
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

        RHIFrameScopeRAII createFrameScope() RHI_OVERRIDE;

        RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc&,
            const std::string& name = ""
        ) RHI_OVERRIDE;
        RHITextureRAII createTexture(
            const RHITextureCreateDesc&,
            const std::string& name = ""
        ) RHI_OVERRIDE;
        RHISamplerRAII createSampler(
            const RHISamplerState&
        ) RHI_OVERRIDE;

        RHIGraphicsPipelineStateRAII createPipelineState(
            const RHIGraphicsPipelineStateDesc&,
            const std::string& name = ""
        ) RHI_OVERRIDE;
        RHIComputePipelineStateRAII createPipelineState(
            const RHIComputePipelineStateDesc&,
            const std::string& name = ""
        ) RHI_OVERRIDE;

        RHISwapchainRAII createSwapchain(
            const RHISwapchainCreateDesc&
        ) RHI_OVERRIDE; 

        RHICommandListRAII createCommandList() RHI_OVERRIDE;

        RHIFenceRAII createFence(uint64_t initialValue = 0) RHI_OVERRIDE;

        RHICapabilities getCapabilities() const noexcept RHI_OVERRIDE;

        void submit(RHICommandList&, RHISwapchain*) noexcept RHI_OVERRIDE;

        void* getNative() noexcept RHI_OVERRIDE;
        void* getContextOrQueue() noexcept RHI_OVERRIDE;
    };
}
