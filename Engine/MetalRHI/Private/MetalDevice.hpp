#pragma once

#include "FastPimpl.hpp"
#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class MetalDevice final: public RHIDevice{
    private:
        class Impl;
        FastPimpl<Impl, 600, 8> impl;

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

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;
        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;

        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;

        RHICommandListRAII CreateCommandList() RHI_OVERRIDE;

        RHIFenceRAII CreateFence(u64 initialValue = 0) RHI_OVERRIDE;

        void SignalFence(RHIFence&, u64 value) RHI_OVERRIDE;

        void Submit(
            std::span<RHICommandList*>,
            RHIFence&
        ) RHI_OVERRIDE;
        void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&,
            RHIFence&
        ) RHI_OVERRIDE;

        u64& GetFrameIndexRef() noexcept RHI_OVERRIDE;

        RHICapabilities GetCapabilities() const noexcept RHI_OVERRIDE;
    };
}
