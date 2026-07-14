#pragma once

#include "RHIAPI.hpp"
#include "RHICommandList.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Buffer;
    class DX12Texture;
    class DX12GraphicsPipelineState;
    class DX12ComputePipelineState;

    class DX12CommandList: public RHICommandList{
    private:
        CommandQueue& commandQueue;
        RootSignature& rootSignature;
        CommandAllocatorRAII commandAllocators[RHI_FRAMES_IN_FLIGHT];
        const u64& frameIndex;

        CommandListRAII commandList = nullptr;
        // simulate command recording
        bool inComputePass = false, inBlitPass = false;

        DescriptorHeapAllocator& cbvsrvuavHeap;
        DescriptorHeapAllocator& rtvHeap;
        DescriptorHeapAllocator& dsvHeap;
        DescriptorHeapAllocator& samplerHeap;
        // TODO. Add REAL bindless api
        DX12GraphicsPipelineState* currentGraphicsPSO = nullptr;
        DX12ComputePipelineState* currentComputePSO = nullptr;

    public:
        DX12CommandList(
            Device&,
            CommandQueue&,
            RootSignature&,
            const u64& frameIndex,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            DescriptorHeapAllocator& samplerHeap
        );
        ~DX12CommandList();

        void Begin() RHI_OVERRIDE;
        void Close() RHI_OVERRIDE;

        void BeginRenderPass(const RHIRenderPassDesc&) RHI_OVERRIDE;
        void EndRenderPass() RHI_OVERRIDE;

        void SetPipelineState(RHIGraphicsPipelineState& pso) RHI_OVERRIDE;

        void SetVertexBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 stride,
            u32 offset
        ) RHI_OVERRIDE;

        void SetIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            u32 offset
        ) RHI_OVERRIDE;

        void SetPushGraphicsConstants(
            const void* data,
            u32 size
        ) RHI_OVERRIDE;

        void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void SetViewport(const RHIViewport& viewport) RHI_OVERRIDE;
        void SetScissorRect(const RHIScissorRect& scissor) RHI_OVERRIDE;

        void Draw(
            u32 vertexCount,
            u32 instanceCount = 1,
            u32 startVertex = 0,
            u32 startInstance = 0
        ) RHI_OVERRIDE;

        void DrawIndexed(
            u32 indexCount,
            u32 instanceCount = 1,
            u32 startIndex = 0,
            i32 baseVertex = 0,
            u32 startInstance = 0
        ) RHI_OVERRIDE;

        void BeginCompute() noexcept RHI_OVERRIDE;
        void EndCompute() noexcept RHI_OVERRIDE;

        void SetPipelineState(RHIComputePipelineState& pso) RHI_OVERRIDE;

        void SetPushComputeConstants(
            const void* data,
            u32 size
        ) RHI_OVERRIDE;

        void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void Dispatch(Size3D gridSize) RHI_OVERRIDE;

        void BeginBlit() noexcept RHI_OVERRIDE;
        void EndBlit() noexcept RHI_OVERRIDE;

        void Copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            usize srcOffset,
            usize dstOffset,
            usize size
        ) RHI_OVERRIDE;

        void Copy(
            RHITexture& src,
            RHITexture& dst
        ) RHI_OVERRIDE;

        void Copy(
            RHIBuffer& src,
            u64 srcOffset,   // align 512 (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)
            u32 srcRowPitch, // align 256 (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
            RHITexture& dst,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) RHI_OVERRIDE;

        void TransitionBarrier(
            RHITexture&,
            RHIBarrierSync syncAfter,
            RHIBarrierAccess accessAfter,
            RHIBarrierLayout layoutAfter
        ) RHI_OVERRIDE;

        void TransitionBarrier(
            RHIBuffer&,
            RHIBarrierSync syncAfter,
            RHIBarrierAccess accessAfter
        ) RHI_OVERRIDE;

        void WaitUntilCompleted() RHI_OVERRIDE;

        void BeginEvent(CStr name) RHI_OVERRIDE;
        void EndEvent() RHI_OVERRIDE;
        void SetMarker(CStr name) RHI_OVERRIDE;

        CommandList* Get() noexcept{ return commandList.Get(); }
        void* GetNative() noexcept RHI_OVERRIDE{ return Get(); }

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % RHI_FRAMES_IN_FLIGHT
            );
        }
    };
}
