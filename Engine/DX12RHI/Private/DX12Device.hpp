#pragma once

#include <functional>
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
        static constexpr usize implSize = 264;
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

        void Submit(std::span<RHICommandList*>) RHI_OVERRIDE;
        void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&
        ) RHI_OVERRIDE;

        u64 GetSubmittedFrame() const noexcept RHI_OVERRIDE;
        u64 GetCompletedFrame() const noexcept RHI_OVERRIDE;
        void WaitFrame(u64 value) RHI_OVERRIDE;
        void WaitIdle() RHI_OVERRIDE;

        void DeferRetire(std::move_only_function<void()> reclaim) RHI_OVERRIDE;

        u64& GetFrameIndexRef() noexcept RHI_OVERRIDE;

        RHICapabilities GetCapabilities() const noexcept RHI_OVERRIDE;

        UINT64 QueryUploadLayout(
            RHITexture&,
            std::span<RHISubresourceLayout> out
        );
    };
}
