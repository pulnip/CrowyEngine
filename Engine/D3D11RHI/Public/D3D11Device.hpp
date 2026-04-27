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

        RHIFrameScopePtr createFrameScope() noexcept RHI_OVERRIDE;

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHIBufferViewPtr createBufferView(
            const RHIBuffer&,
            const RHIBufferViewDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHITexturePtr createTexture(
            const RHITextureCreateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHITextureViewPtr createTextureView(
            const RHITexture&,
            const RHITextureViewDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHIShaderPtr createShader(
            const RHIShaderCreateDesc&
        ) RHI_OVERRIDE;
        RHISamplerPtr createSampler(
            const RHISamplerState&
        ) noexcept RHI_OVERRIDE;

        RHIGraphicsPipelineStatePtr createPipelineState(
            const RHIGraphicsPipelineStateDesc&,
            const std::string& name = ""
        ) noexcept RHI_OVERRIDE;
        RHIComputePipelineStatePtr createPipelineState(
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
        void* getContextOrQueue() noexcept RHI_OVERRIDE;
    };
}
