#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include <d3dx12/d3dx12_barriers.h>

#include "DX12Definitions.hpp"
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"

namespace Crowy
{
    class DX12Buffer;
    class DX12Fence;
    class DX12Texture;
    class DX12GraphicsPipelineState;
    class DX12ComputePipelineState;

    class DX12CommandList: public RHICommandList {
    public:
        // begin at 2 * slot, end at 2 * slot + 1; holds the buffer so the
        // device can read it after the list is gone
        struct TimestampSlot {
            BufferRAII readback;
            u32 slot = 0;
        };

    private:
        using Super = RHICommandList;

    private:
        // record-time CPU bookkeeping for pairing barrier halves, not GPU state
        // tracking: list-local, so parallel recording keeps working
        template<typename Barrier>
        struct PendingRelease {
            Barrier barrier;
            bool consumed = false;
        };

        CommandQueue& commandQueue;
        RootSignature& rootSignature;
        CommandSignature& drawSignature;
        CommandSignature& drawIndexedSignature;
        // each allocator carries the serial of the batch that last ran its
        // commands, so reuse waits on exactly that
        struct AllocatorSlot {
            CommandAllocatorRAII allocator;
            u64 serial = 0;
        };
        std::array<AllocatorSlot, FramesInFlight> allocators;
        u32 cursor = 0;
        DX12Fence& serialFence;
        QueryHeapRAII timestamps;
        BufferRAII timestampReadback;

        CommandListRAII commandList = nullptr;

        // releases park here and each acquire fuses its matched pair into one
        // Enhanced Barrier recorded before the consumer
        std::unordered_map<
            const RHITexture*,
            std::vector<PendingRelease<RHITextureBarrier>>>
            pendingTextureReleases;
        std::unordered_map<const RHIBuffer*, PendingRelease<RHIBufferBarrier>>
            pendingBufferReleases;

        DX12DescriptorHeapAllocator& cbvsrvuavHeap;
        DX12DescriptorHeapAllocator& rtvHeap;
        DX12DescriptorHeapAllocator& dsvHeap;
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
            DX12DescriptorHeapAllocator& cbvsrvuavHeap,
            DX12DescriptorHeapAllocator& rtvHeap,
            DX12DescriptorHeapAllocator& dsvHeap
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

        void SetPushGraphicsConstants(const void* data, u32 size) RHI_OVERRIDE;

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

        void SetPushComputeConstants(const void* data, u32 size) RHI_OVERRIDE;

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

        void Copy(RHITexture& src, RHITexture& dst) RHI_OVERRIDE;

        void Copy(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) RHI_OVERRIDE;

        void Copy(
            RHITexture& src,
            RHIBuffer& dst,
            u64 dstOffset,
            u32 dstRowPitch,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) RHI_OVERRIDE;

        using RHICommandList::Copy;

        void BeginEvent(CStr name) RHI_OVERRIDE;
        void EndEvent() RHI_OVERRIDE;
        void SetMarker(CStr name) RHI_OVERRIDE;

        CommandList* Get() noexcept { return commandList.Get(); }

        // `serial` is what the batch signals once these commands have run
        void MarkSubmitted(u64 serial) noexcept {
            allocators[cursor].serial = serial;
        }
        TimestampSlot SubmittedTimestamps() const {
            return {timestampReadback, cursor};
        }

    private:
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
