#pragma once

#include "FastPimpl.hpp"
#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class DX12Sampler;

    class DX12Device: public RHIDevice{
    private:
        class Impl;
        static constexpr usize implSize = 176;
        static constexpr usize implAlign = 8;
        FastPimpl<Impl, implSize, implAlign> impl;

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
        RAII<DX12Sampler> CreateSampler(
            const RHISamplerState&
        );

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

        UINT64 QueryUploadLayout(
            RHITexture&,
            std::span<RHISubresourceLayout> out
        );
    };
}
