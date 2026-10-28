#pragma once

#include <array>
#include <variant>
#include <vector>

#include <d3dx12/d3dx12_barriers.h>

#include "DX12Definitions.hpp"
#include "RHICommandList.hpp"
#include "RHIReleaseLedger.hpp"

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
            RefPtr<Buffer> readback;
            u32 slot = 0;
        };

    private:
        CommandQueue& commandQueue;
        RootSignature& rootSignature;
        CommandSignature& drawSignature;
        CommandSignature& drawIndexedSignature;
        // each allocator carries the serial of the batch that last ran its
        // commands, so reuse waits on exactly that
        struct AllocatorSlot {
            RefPtr<CommandAllocator> allocator;
            u64 serial = 0;
        };
        std::array<AllocatorSlot, FramesInFlight> allocators;
        u32 cursor = 0;
        DX12Fence& serialFence;
        RefPtr<QueryHeap> timestamps;
        RefPtr<Buffer> timestampReadback;

        RefPtr<CommandList> commandList = nullptr;

        // releases park here and each acquire fuses its matched pair into one
        // Enhanced Barrier recorded before the consumer
        RHIReleaseLedger<std::monostate> releases;

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

        CommandList* Get() noexcept { return commandList.Get(); }

        // `serial` is what the batch signals once these commands have run
        void MarkSubmitted(u64 serial) noexcept {
            allocators[cursor].serial = serial;
        }
        TimestampSlot SubmittedTimestamps() const {
            return {timestampReadback, cursor};
        }

    private:
        void begin() override;
        void close() override;

        void beginRenderPass(
            const RHIRenderPassDesc&,
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) override;
        void endRenderPass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) override;

        void setGraphicsPipelineState(RHIGraphicsPipelineState& pso) override;

        void setPushGraphicsConstants(const void* data, u32 size) override;

        void setGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset
        ) override;

        void setViewport(const RHIViewport& viewport) override;
        void setScissorRect(const RHIScissorRect& scissor) override;

        void draw(
            u32 vertexCount,
            u32 instanceCount,
            u32 startVertex,
            u32 startInstance
        ) override;

        void drawIndexed(
            const RHIIndexBufferView& indices,
            u32 indexCount,
            u32 instanceCount,
            u32 startIndex,
            i32 baseVertex,
            u32 startInstance
        ) override;

        void executeIndirect(const DrawBatch&) override;
        void executeIndirectIndexed(const DrawBatchIndexed&) override;

        void beginComputePass(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) override;
        void endComputePass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) override;

        void setComputePipelineState(RHIComputePipelineState& pso) override;

        void setPushComputeConstants(const void* data, u32 size) override;

        void setComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset
        ) override;

        void dispatch(Size3D gridSize) override;

        void beginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) override;
        void endBlitPass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) override;

        void dispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers,
            std::span<const RHIBufferBarrier> bufferBarriers
        ) override;

        void copyBuffer(
            RHIBuffer& src,
            RHIBuffer& dst,
            usize srcOffset,
            usize dstOffset,
            usize size
        ) override;

        void copyTexture(RHITexture& src, RHITexture& dst) override;

        void copyBufferToTexture(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            const RHITextureRegion& region,
            u32 mipLevel,
            u32 arraySlice
        ) override;

        void copyTextureToBuffer(
            RHITexture& src,
            RHIBuffer& dst,
            u64 dstOffset,
            u32 dstRowPitch,
            const RHITextureRegion& region,
            u32 mipLevel,
            u32 arraySlice
        ) override;

        void beginEvent(CStr name) override;
        void endEvent() override;
        void setMarker(CStr name) override;

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
