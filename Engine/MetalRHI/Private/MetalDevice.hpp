#pragma once

#include <functional>
#include "FastPimpl.hpp"
#include "RHIAPI.hpp"
#include "RHIFWD.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class MetalDevice final: public RHIDevice{
    private:
        class Impl;
        FastPimpl<Impl, 1032, 8> impl;

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

        RHIBufferSlice AllocateTransient(u32 size, u32 align) RHI_OVERRIDE;

        RHICapabilities GetCapabilities() const noexcept RHI_OVERRIDE;
    };
}
