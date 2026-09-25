#pragma once

#include <array>
#include <unordered_map>
#include <vector>
#include <d3dx12/d3dx12_barriers.h>
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Buffer;
    class DX12Fence;
    class DX12Texture;
    class DX12GraphicsPipelineState;
    class DX12ComputePipelineState;

    class DX12CommandList: public RHICommandList{
    public:
        // where a submitted recording leaves its two GPU timestamps, begin
        // at 2 * slot and end at 2 * slot + 1. It holds the buffer, so the
        // device can read it after the list itself is gone
        struct TimestampSlot{
            BufferRAII readback;
            u32 slot = 0;
        };

    private:
        using Super = RHICommandList;

    private:
        // record-time CPU bookkeeping for pairing barrier halves - NOT GPU
        // state tracking. it stays commandlist-local so parallel recording
        // keeps working; the GPU ordering comes only from recorded barriers.
        template<typename Barrier>
        struct PendingRelease{
            Barrier barrier;
            // at least one acquire already fused this edge (multi-consumer)
            bool consumed = false;
        };

        CommandQueue& commandQueue;
        RootSignature& rootSignature;
        CommandSignature& drawSignature;
        CommandSignature& drawIndexedSignature;
        // Begin() takes the next allocator in the ring. Each carries the
        // serial of the batch that last ran its commands, so reuse waits on
        // exactly that and nothing here needs to know which frame it is
        struct AllocatorSlot{
            CommandAllocatorRAII allocator;
            u64 serial = 0;
        };
        std::array<AllocatorSlot, RHI_FRAMES_IN_FLIGHT> allocators;
        u32 cursor = 0;
        DX12Fence& serialFence;
        // a begin and an end timestamp per allocator slot, following the
        // cursor; Close resolves the pair into the readback buffer
        QueryHeapRAII timestamps;
        BufferRAII timestampReadback;

        CommandListRAII commandList = nullptr;

        // v1 fuse strategy: releases park here and each acquire fuses its
        // matched pair into one Enhanced Barrier recorded before the consumer
        std::unordered_map<
            const RHITexture*,
            std::vector<PendingRelease<RHITextureBarrier>>
        > pendingTextureReleases;
        std::unordered_map<
            const RHIBuffer*,
            PendingRelease<RHIBufferBarrier>
        > pendingBufferReleases;

        DescriptorHeapAllocator& cbvsrvuavHeap;
        DescriptorHeapAllocator& rtvHeap;
        DescriptorHeapAllocator& dsvHeap;
        DescriptorHeapAllocator& samplerHeap;
        // TODO. Add REAL bindless api
        DX12GraphicsPipelineState* currentGraphicsPSO = nullptr;
        DX12ComputePipelineState* currentComputePSO = nullptr;

        std::vector<CD3DX12_TEXTURE_BARRIER> textureBarrierScratch;
        std::vector<CD3DX12_BUFFER_BARRIER> bufferBarrierScratch;

    public:
        DX12CommandList(
            Device&,
            CommandQueue&,
            RootSignature&,
            CommandSignature& drawSignature,
            CommandSignature& drawIndexedSignature,
            DX12Fence& serialFence,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            DescriptorHeapAllocator& samplerHeap
        );
        ~DX12CommandList();

        void Begin() RHI_OVERRIDE;
        void Close() RHI_OVERRIDE;

        void BeginRenderPass(
            const RHIRenderPassDesc&,
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) RHI_OVERRIDE;
        void EndRenderPass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) RHI_OVERRIDE;

        void SetPipelineState(RHIGraphicsPipelineState& pso) RHI_OVERRIDE;

        void SetVertexBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 stride,
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
            const RHIIndexBufferView& indices,
            u32 indexCount,
            u32 instanceCount = 1,
            u32 startIndex = 0,
            i32 baseVertex = 0,
            u32 startInstance = 0
        ) RHI_OVERRIDE;

        void ExecuteIndirect(const DrawBatch&) RHI_OVERRIDE;
        void ExecuteIndirectIndexed(const DrawBatchIndexed&) RHI_OVERRIDE;

        void BeginComputePass(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) RHI_OVERRIDE;
        void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) RHI_OVERRIDE;

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

        void BeginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) RHI_OVERRIDE;
        void EndBlitPass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) RHI_OVERRIDE;

        void DispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers,
            std::span<const RHIBufferBarrier> bufferBarriers
        ) RHI_OVERRIDE;

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
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) RHI_OVERRIDE;

        using RHICommandList::Copy;

        void BeginEvent(CStr name) RHI_OVERRIDE;
        void EndEvent() RHI_OVERRIDE;
        void SetMarker(CStr name) RHI_OVERRIDE;

        CommandList* Get() noexcept{ return commandList.Get(); }

        // the device stamps the recording as it executes it: `serial` is what
        // the batch signals once these commands have run
        void MarkSubmitted(u64 serial) noexcept{ allocators[cursor].serial = serial; }
        // the timestamps of the recording just submitted, readable once its
        // batch's serial has completed
        TimestampSlot SubmittedTimestamps() const{ return {timestampReadback, cursor}; }

    private:
        // record the whole edge as one Enhanced Barrier into the scratch
        void pushFused(const RHITextureBarrier&);
        void pushFused(const RHIBufferBarrier&);
        void flushBarrierScratch();

        // End*Pass halves: self-contained releases fuse on the spot,
        // real edges park in the pending map
        void queueRelease(const RHITextureBarrier&);
        void queueRelease(const RHIBufferBarrier&);
        void queueReleases(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>
        );

        // Begin*Pass halves: fuse against the matching pending release,
        // or record a self-contained acquire
        void applyAcquire(const RHITextureBarrier&);
        void applyAcquire(const RHIBufferBarrier&);
        void applyAcquires(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>
        );

        // releases nobody acquired complete at Close - the hand-off point
        // for consumers living in later submissions
        void flushPendingReleases();
    };
}
