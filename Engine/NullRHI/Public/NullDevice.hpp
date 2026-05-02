#pragma once

#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
#ifdef USE_STATIC_RHI
    #include "RHIDefinitions.hpp"
#else
    #include "RHIDevice.hpp"
#endif

namespace Crowy
{
    class NullDevice
#ifndef USE_STATIC_RHI
        : public RHIDevice
#endif
    {
    public:
        NullDevice() = default;
        ~NullDevice() = default;

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

        void* getNative() noexcept RHI_OVERRIDE{ return nullptr; }
    };
}
