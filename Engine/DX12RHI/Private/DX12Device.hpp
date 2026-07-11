#pragma once

#include "DX12CommandList.hpp"
#include "DX12PipelineState.hpp"
#include "DX12Swapchain.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class DX12Device{
    private:
        class Impl;
        RAII<Impl> impl;

    public:
        DX12Device();
        ~DX12Device();

        RHIFrameScopeRAII CreateFrameScope();

        RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc&,
            StrView name = {}
        );
        RHITextureRAII CreateTexture(
            const RHITextureCreateDesc&,
            StrView name = {}
        );
        RHISamplerRAII CreateSampler(
            const RHISamplerState&
        );

        RAII<DX12GraphicsPipelineState> CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        );
        RAII<DX12ComputePipelineState> CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        );

        RAII<DX12Swapchain> CreateSwapchain(
            const RHISwapchainCreateDesc&
        );

        RAII<DX12CommandList> CreateCommandList();

        RHIFenceRAII CreateFence(u64 initialValue = 0);

        RHICapabilities GetCapabilities() const noexcept;

        void SignalFence(RHIFence&, u64);

        void Submit(std::span<DX12CommandList*>);

        u64& GetFrameIndexRef() noexcept;

        UINT64 QueryUploadLayout(
            RHITexture&,
            std::span<RHISubresourceLayout> out
        );
    };
}
