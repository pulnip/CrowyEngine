#pragma once

#include <array>
#include <unordered_map>
#include <variant>
#include <vector>
#include <Metal/MTLCommandBuffer.hpp>
namespace MTL{
    class SharedEvent;
}
#include <Metal/MTLEvent.hpp>
#include <Metal/MTLFence.hpp>
#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLStageInputOutputDescriptor.hpp>
#include <Metal/MTLTypes.hpp>
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"

namespace Crowy
{
    class MetalCommandList final: public RHICommandList{
    private:
        using Super = RHICommandList;

        struct RenderPassState{
            MTL::RenderCommandEncoder* encoder = nullptr;

            // snapshotted from the PSO at SetPipelineState
            MTL::PrimitiveType topology = MTL::PrimitiveType::PrimitiveTypeTriangle;
            u32 vsUsedBufferMask = 0;
            u32 fsUsedBufferMask = 0;

            // snapshotted from the Index Buffer at SetIndexBuffer
            MTL::Buffer* indexBuffer = nullptr;
            u32 indexBufferOffset = 0;
            MTL::IndexType indexFormat = MTL::IndexTypeUInt32;

            // cache for lazy binding of PushConstant
            std::array<u8, RHI_PUSH_CONSTANT_BYTES> pushConstants;
            u32 pushConstantSize = 0;
            bool pushDirty = false;

            // cache for lazy binding of ConstantBuffer
            struct ConstantBufferBinding{
                MTL::Buffer* buffer = nullptr;
                u32 offset = 0;
            };
            std::array<ConstantBufferBinding, RHI_NUM_DIRECT_CBS> constantBuffers{};
            u32 cbDirtyMask = 0;
        };

        struct ComputePassState{
            MTL::ComputeCommandEncoder* encoder = nullptr;

            MTL::Size threadsPerThreadgroup = {0, 0, 0};
        };

        struct BlitPassState{
            MTL::BlitCommandEncoder* encoder = nullptr;
        };

        // record-time CPU bookkeeping for pairing barrier halves
        // - NOT GPU state tracking.
        // it stays commandlist-local so parallel recording keeps working;
        // the GPU ordering comes only from the fence edges.
        template<typename Barrier>
        struct PendingRelease{
            Barrier barrier;
            // updated by the producer's encoder; every acquire waits on it
            // (multi-consumer means several waits on one fence - legal)
            MTL::Fence* fence;
            // an entry no acquire ever matched is a cross-submission
            // hand-off - the device gates the following waves on it
            bool consumed = false;
        };

        // one wait per producer fence at Begin*Pass, syncAfter unioned
        // across the acquires that share it
        struct FenceWait{
            MTL::Fence* fence;
            RHIBarrierSync syncAfter;
        };

    private:
        MTL::CommandQueue* commandQueue = nullptr;
        // cross-submission acquires and un-acquired hand-off releases record no fence at all
        // - they rely on the queue-global sync scope D3D12 barriers get for free.
        // the device signals this event once per submitted wave;
        // a lazy wait on the last signaled value restores that scope
        // exactly where a recording depends on it,
        // so passes before that point keep overlapping the previous wave on the GPU:
        //  - before the first pass carrying a cross-submission acquire
        //  - at Begin, while a hand-off wave may still be on the GPU
        //    (the hangover window below)
        MTL::Event* submissionEvent = nullptr;
        // last value actually signaled on the event.
        // tracked separately from the device frameIndex, which callers may bump out-of-band
        // (FramePacer::WaitForIdle) - a gate must never wait a value
        // nothing signals
        const u64& submissionSerial;
        // serial of the last wave that left un-acquired hand-off releases;
        // 0 = none yet
        const u64& handoffSerial;

        NS::SharedPtr<MTL::CommandBuffer> commandBuffer;

        std::variant<
            RenderPassState,
            ComputePassState,
            BlitPassState,
            std::monostate
        > passState = std::monostate{};

        bool isRecording = false;
        // at most one submission-event wait per recording; everything
        // encoded after it is ordered behind the prior waves
        bool submissionWaitEncoded = false;

        // one fence per End*Pass release batch (fences are encoder-grained,
        // not resource-grained). reused across recordings: a stale update
        // seen by a later wait only adds an already-satisfied edge.
        std::vector<NS::SharedPtr<MTL::Fence>> fencePool;
        usize fenceCursor = 0;

        std::unordered_map<
            const RHITexture*,
            std::vector<PendingRelease<RHITextureBarrier>>
        > pendingTextureReleases;
        std::unordered_map<
            const RHIBuffer*,
            PendingRelease<RHIBufferBarrier>
        > pendingBufferReleases;

        std::vector<FenceWait> waitScratch;

        // write recorded-but-dirty constants to the encoder; every
        // draw entry point calls this before encoding
        static void flush(RenderPassState&);
        // (re)bind recorded graphics constants through the
        // snapshotted used-buffer masks
        static void applyPushConstants(RenderPassState&);
        static void applyConstantBuffer(RenderPassState&, u32 slot);

    public:
        MetalCommandList(
            MTL::CommandQueue*,
            MTL::Event* submissionEvent,
            const u64& submissionSerial,
            const u64& handoffSerial
        );
        ~MetalCommandList();

        void Begin() RHI_OVERRIDE;
        void Close() RHI_OVERRIDE;

        void BeginRenderPass(
            const RHIRenderPassDesc&,
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) RHI_OVERRIDE;
        void EndRenderPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) RHI_OVERRIDE;

        void SetPipelineState(RHIGraphicsPipelineState&) RHI_OVERRIDE;

        void SetVertexBuffer(
            RHIBuffer&,
            u32 slot,
            u32 stride,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void SetIndexBuffer(
            RHIBuffer&,
            RHIIndexFormat format = RHIIndexFormat::UInt32,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void SetPushGraphicsConstants(
            const void* data,
            u32 size
        ) RHI_OVERRIDE;

        void SetGraphicsConstantBuffer(
            RHIBuffer&,
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

        void ExecuteIndirect(const DrawBatch&) RHI_OVERRIDE;

        void BeginComputePass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) RHI_OVERRIDE;
        void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) RHI_OVERRIDE;

        void SetPipelineState(RHIComputePipelineState& pso) RHI_OVERRIDE;

        void SetPushComputeConstants(
            const void* data,
            u32 size
        ) RHI_OVERRIDE;

        void SetComputeConstantBuffer(
            RHIBuffer&,
            u32 slot,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void Dispatch(Size3D gridSize) RHI_OVERRIDE;

        void DispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers = {},
            std::span<const RHIBufferBarrier> bufferBarriers = {}
        ) RHI_OVERRIDE;

        void BeginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) RHI_OVERRIDE;
        void EndBlitPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
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
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) RHI_OVERRIDE;

        using RHICommandList::Copy;

        void BeginEvent(CStr name) RHI_OVERRIDE;
        void EndEvent() RHI_OVERRIDE;

        void SetMarker(CStr name) RHI_OVERRIDE;

        auto Get() const noexcept{ return commandBuffer.get(); }

        // any release parked in this recording that no acquire matched -
        // its consumers live in later submissions, which the device must
        // gate while this wave may still be on the GPU
        bool HasUnconsumedReleases() const noexcept;

    private:
        MTL::Fence* acquireFence();

        // encode the lazy submission-event wait (once per recording);
        // legal only between encoders
        void encodeSubmissionWait();

        // Begin*Pass halves: match acquires against the pending maps and
        // fill waitScratch, one entry per distinct producer fence - this is
        // where a non-adjacent A-B-C dependency picks A's fence, not B's.
        // pending entries stay: extra consumers wait on the same fence.
        MTL::Fence* findPending(const RHITextureBarrier&);
        MTL::Fence* findPending(const RHIBufferBarrier&);
        void gatherWaits(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>
        );

        // End*Pass halves: park real edges in the pending maps, all tagged
        // with one fresh fence the caller must update behind the producer.
        // returns nullptr when every release was self-contained
        // (syncAfter == None - later ordering is guaranteed elsewhere and
        // layout is a Metal no-op, so nothing records at all).
        void parkRelease(const RHITextureBarrier&, MTL::Fence*);
        void parkRelease(const RHIBufferBarrier&, MTL::Fence*);
        MTL::Fence* parkReleases(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>,
            RHIBarrierSync& syncBeforeUnion
        );
    };
}
