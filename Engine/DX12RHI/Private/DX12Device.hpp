#pragma once

#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class DX12Device: public RHIDevice{
    private:
        class Impl;
        RAII<Impl> impl;

    public:
        DX12Device();
        ~DX12Device();

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

        RAII<RHIGraphicsPipelineState> CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;
        RAII<RHIComputePipelineState> CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;

        RAII<RHISwapchain> CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView name = {}
        ) RHI_OVERRIDE;

        RAII<RHICommandList> CreateCommandList() RHI_OVERRIDE;

        RHIFenceRAII CreateFence(u64 initialValue = 0) RHI_OVERRIDE;

        void SignalFence(RHIFence&, u64) RHI_OVERRIDE;

        void Submit(std::span<RHICommandList*>) RHI_OVERRIDE;

        u64& GetFrameIndexRef() noexcept RHI_OVERRIDE;

        RHICapabilities GetCapabilities() const noexcept RHI_OVERRIDE;

        NativeDeviceHandle Get() noexcept RHI_OVERRIDE;

        UINT64 QueryUploadLayout(
            RHITexture&,
            std::span<RHISubresourceLayout> out
        );
    };
}
