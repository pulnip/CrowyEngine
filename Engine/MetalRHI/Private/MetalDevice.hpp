#pragma once

#include <functional>

#include "FastPimpl.hpp"
#include "RHIDevice.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    class MetalDevice final: public RHIDevice {
    private:
        class Impl;
        FastPimpl<Impl, 3008, 8> impl;

    public:
        MetalDevice();
        ~MetalDevice();

        RHIFrameScopeRAII CreateFrameScope() override;

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        ) override;
        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        ) override;

        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView name = {}
        ) override;

        RHICommandListRAII CreateCommandList() override;

        u64 GetCompletedFrame() const noexcept override;
        std::optional<f64> GetGPUFrameTime(u64 frame) const noexcept override;
        void WaitFrame(u64 frame) override;
        void WaitIdle() override;

        void DeferRetire(std::move_only_function<void()> reclaim) override;

        RHIBufferSlice AllocateTransient(u32 size, u32 align) override;

        RHICapabilities GetCapabilities() const noexcept override;

    private:
        void submit(std::span<RHICommandList*>, u64 frame) override;
        void submitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&,
            u64 frame
        ) override;

        RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc&,
            StrView name
        ) override;
        RHITextureRAII createTexture(
            const RHITextureCreateDesc&,
            StrView name
        ) override;
    };
}
