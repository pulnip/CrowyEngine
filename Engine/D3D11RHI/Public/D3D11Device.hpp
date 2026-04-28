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

        RHIFrameScopeRAII createFrameScope() noexcept RHI_OVERRIDE;

        RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHITextureRAII createTexture(
            const RHITextureCreateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHISamplerRAII createSampler(
            const RHISamplerState&
        ) noexcept RHI_OVERRIDE;

        RHIGraphicsPipelineStateRAII createPipelineState(
            const RHIGraphicsPipelineStateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHIComputePipelineStateRAII createPipelineState(
            const RHIComputePipelineStateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;

        RHISwapchainRAII createSwapchain(
            const RHISwapchainCreateDesc&
        ) noexcept RHI_OVERRIDE; 

        RHICommandListRAII createCommandList() noexcept RHI_OVERRIDE;

        RHIFenceRAII createFence(uint64_t initialValue = 0) noexcept RHI_OVERRIDE;

        RHICapabilities getCapabilities() const noexcept RHI_OVERRIDE;

        void submit(RHICommandList&, RHISwapchain*) noexcept RHI_OVERRIDE;

        void* getNative() noexcept RHI_OVERRIDE;
        void* getContextOrQueue() noexcept RHI_OVERRIDE;
    };
}
