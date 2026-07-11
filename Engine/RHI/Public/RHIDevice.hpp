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
        SMOL_DECLARE_INTERFACE(RHIDevice)

        virtual RHIFrameScopeRAII CreateFrameScope() = 0;

        virtual RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc&,
            StrView name = {}
        ) = 0;
        virtual RHITextureRAII CreateTexture(
            const RHITextureCreateDesc&,
            StrView name = {}
        ) = 0;
        virtual RHISamplerRAII CreateSampler(
            const RHISamplerState&
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
            const RHISwapchainCreateDesc&
        ) = 0;

        virtual RHICommandListRAII CreateCommandList() = 0;

        virtual RHIFenceRAII CreateFence(u64 initialValue = 0) = 0;

        virtual void SignalFence(RHICommandList&, RHIFence&, u64 value) = 0;

        virtual void Submit(RHICommandList&) = 0;

        virtual RHICapabilities GetCapabilities() const noexcept = 0;

        // return native type for UI
        // in Metal, MTL::Device
        virtual NativeDeviceHandle Get() noexcept = 0;

        virtual RHICommandList& GetMainCmdList() noexcept = 0;
    };

#if defined(_WIN32)
    RHIDeviceRAII CreateDevice(RHIBackend backend = RHIBackend::DirectX12);
#elif defined(__APPLE__)
    RHIDeviceRAII CreateDevice(RHIBackend backend = RHIBackend::Metal);
#else
    RHIDeviceRAII CreateDevice(RHIBackend backend = RHIBackend::WebGPU);
#endif
}
