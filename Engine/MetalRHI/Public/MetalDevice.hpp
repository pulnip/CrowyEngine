#pragma once

#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class MetalDevice final: public RHIDevice{
    private:
        class Impl;
        RAII<Impl> impl;

    public:
        MetalDevice();
        ~MetalDevice();

        RHIFrameScopeRAII CreateFrameScope() RHI_OVERRIDE;

        RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;
        RHITextureRAII CreateTexture(
            const RHITextureCreateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;
        RHISamplerRAII CreateSampler(
            const RHISamplerState&
        ) RHI_OVERRIDE;

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;
        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;

        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&
        ) RHI_OVERRIDE;

        RHICommandListRAII CreateCommandList() RHI_OVERRIDE;

        RHIFenceRAII CreateFence(u64 initialValue = 0) RHI_OVERRIDE;

        void SignalFence(RHICommandList&, RHIFence&, u64 value) RHI_OVERRIDE;

        RHICapabilities GetCapabilities() const noexcept RHI_OVERRIDE;

        void Submit(RHICommandList&) RHI_OVERRIDE;

        NativeDeviceHandle Get() noexcept RHI_OVERRIDE;
        RHICommandList& GetMainCmdList() noexcept RHI_OVERRIDE;
    };
}
