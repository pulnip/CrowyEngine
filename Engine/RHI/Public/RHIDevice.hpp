#pragma once

#include "Semantics.hpp"
#include "Primitives.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    using NativeDeviceHandle = void*;

    class RHIDevice{
    public:
        CROWY_DECLARE_INTERFACE(RHIDevice)

        virtual RHIFrameScopeRAII CreateFrameScope() = 0;

        virtual RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc&,
            StrView name = {}
        ) = 0;
        virtual RHITextureRAII CreateTexture(
            const RHITextureCreateDesc&,
            StrView name = {}
        ) = 0;

        virtual RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        ) = 0;
        virtual RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        ) = 0;

        virtual RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView name = {}
        ) = 0;

        virtual RHICommandListRAII CreateCommandList() = 0;

        virtual void Submit(std::span<RHICommandList*>) = 0;
        virtual void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&
        ) = 0;

        // the frame value the last Submit/SubmitAndPresent tagged
        virtual u64 GetSubmittedFrame() const noexcept = 0;
        // the frame value the GPU has actually finished
        virtual u64 GetCompletedFrame() const noexcept = 0;
        // block the CPU until GetCompletedFrame() >= value
        virtual void WaitFrame(u64 value) = 0;
        // block until every submission so far has completed
        virtual void WaitIdle() = 0;

        virtual u64& GetFrameIndexRef() noexcept = 0;

        virtual RHICapabilities GetCapabilities() const noexcept = 0;
    };

#if defined(_WIN32)
    RHIDeviceRAII CreateDevice(RHIBackend backend = RHIBackend::DirectX12);
#elif defined(__APPLE__)
    RHIDeviceRAII CreateDevice(RHIBackend backend = RHIBackend::Metal);
#else
    RHIDeviceRAII CreateDevice(RHIBackend backend = RHIBackend::WebGPU);
#endif
}
