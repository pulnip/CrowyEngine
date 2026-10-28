#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include <Metal/MTLCommandBuffer.hpp>
namespace MTL
{
    class SharedEvent;
}
#include <Metal/MTLEvent.hpp>
#include <Metal/MTLFence.hpp>
#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLStageInputOutputDescriptor.hpp>
#include <Metal/MTLTypes.hpp>

#include "MetalPipelineState.hpp"
#include "MetalUtil.hpp"
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"

namespace Crowy
{
    class MetalCommandList final: public RHICommandList {
    private:
        using Super = RHICommandList;

        struct RenderPassState {
            MTL::RenderCommandEncoder* encoder = nullptr;

            // snapshotted from the PSO at SetPipelineState
            MTL::PrimitiveType topology =
                MTL::PrimitiveType::PrimitiveTypeTriangle;
            u32 vsUsedBufferMask = 0;
            u32 fsUsedBufferMask = 0;

            std::array<u8, PushConstantBytes> pushConstants;
            u32 pushConstantSize = 0;
            bool pushDirty = false;

            struct ConstantBufferBinding {
                MTL::Buffer* buffer = nullptr;
                u32 offset = 0;
            };
            std::array<ConstantBufferBinding, DirectCbCount> constantBuffers{};
            u32 cbDirtyMask = 0;

            // what this encoder already holds, so a pipeline switch or a
            // repeated binding sets only what changed; empty until first set
            struct BufferArgument {
                MTL::Buffer* buffer = nullptr;
                NS::UInteger offset = 0;
            };
            MTL::RenderPipelineState* boundPipeline = nullptr;
            std::optional<MetalRasterState> boundRaster;
            MTL::DepthStencilState* boundDepthStencil = nullptr;
            std::optional<MTL::Viewport> boundViewport;
            std::optional<MTL::ScissorRect> boundScissor;
            // indexed like the buffer and sampler argument tables
            std::array<BufferArgument, MaxBufferArguments> boundVSBuffers{};
            std::array<BufferArgument, MaxBufferArguments> boundFSBuffers{};
            std::array<MTL::SamplerState*, MaxSamplerArguments>
                boundVSSamplers{};
            std::array<MTL::SamplerState*, MaxSamplerArguments>
                boundFSSamplers{};
        };

        struct ComputePassState {
            MTL::ComputeCommandEncoder* encoder = nullptr;

            MTL::Size threadsPerThreadgroup = {0, 0, 0};
        };

        struct BlitPassState {
            MTL::BlitCommandEncoder* encoder = nullptr;
        };

        // record-time CPU bookkeeping for pairing barrier halves, not GPU state
        // tracking: list-local, so parallel recording keeps working
        template<typename Barrier>
        struct PendingRelease {
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
        struct FenceWait {
            MTL::Fence* fence;
            RHIBarrierSync syncAfter;
        };

    private:
        MTL::CommandQueue* commandQueue = nullptr;
        // cross-submission edges record no fence; the device signals this per
        // wave, and a lazy wait on the last value restores queue-global scope
        MTL::Event* submissionEvent = nullptr;
        // the device's serial; every advance of it is also signaled on the
        // event, so a gate never waits on a value nothing signals
        const u64& submissionSerial;
        // serial of the last wave that left un-acquired hand-offs; 0 = none
        const u64& handoffSerial;

        NS::SharedPtr<MTL::CommandBuffer> commandBuffer;

        std::variant<
            RenderPassState,
            ComputePassState,
            BlitPassState,
            std::monostate>
            passState = std::monostate{};

        bool isRecording = false;
        // at most one submission-event wait per recording; everything
        // encoded after it is ordered behind the prior waves
        bool submissionWaitEncoded = false;

        // one fence per End*Pass release batch (fences are encoder-grained);
        // reuse is safe: a stale update only adds a satisfied edge
        std::vector<NS::SharedPtr<MTL::Fence>> fencePool;
        usize fenceCursor = 0;

        std::unordered_map<
            const RHITexture*,
            std::vector<PendingRelease<RHITextureBarrier>>>
            pendingTextureReleases;
        std::unordered_map<const RHIBuffer*, PendingRelease<RHIBufferBarrier>>
            pendingBufferReleases;

        std::vector<FenceWait> waitScratch;

        // write recorded-but-dirty constants to the encoder; every
        // draw entry point calls this before encoding
        static void flush(RenderPassState&);
        // (re)bind recorded graphics constants through the
        // snapshotted used-buffer masks
        static void applyPushConstants(RenderPassState&);
        static void applyConstantBuffer(RenderPassState&, u32 slot);
        // set on the encoder only what differs from what it holds
        static void applyPipeline(
            RenderPassState&,
            const MetalGraphicsPipelineState&
        );
        static void applyRasterState(RenderPassState&, const MetalRasterState&);
        static void bindVertexBuffer(
            RenderPassState&,
            MTL::Buffer*,
            NS::UInteger offset,
            NS::UInteger index
        );
        static void bindFragmentBuffer(
            RenderPassState&,
            MTL::Buffer*,
            NS::UInteger offset,
            NS::UInteger index
        );

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

        void SetPushGraphicsConstants(const void* data, u32 size) RHI_OVERRIDE;

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
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) RHI_OVERRIDE;
        void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) RHI_OVERRIDE;

        void SetPipelineState(RHIComputePipelineState& pso) RHI_OVERRIDE;

        void SetPushComputeConstants(const void* data, u32 size) RHI_OVERRIDE;

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

        auto Get() const noexcept { return commandBuffer.get(); }

        // a parked release no acquire matched: its consumers are in later
        // submissions, which the device gates while this wave may run
        bool HasUnconsumedReleases() const noexcept;

    private:
        MTL::Fence* acquireFence();

        // encode the lazy submission-event wait (once per recording);
        // legal only between encoders
        void encodeSubmissionWait();

        // Begin*Pass halves: one wait per distinct producer fence, so a
        // non-adjacent A-B-C dependency picks A's fence; entries stay parked
        MTL::Fence* findPending(const RHITextureBarrier&);
        MTL::Fence* findPending(const RHIBufferBarrier&);
        void gatherWaits(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>
        );

        // End*Pass halves: park real edges under one fresh fence the caller
        // updates; self-contained releases (syncAfter None) record nothing
        void parkRelease(const RHITextureBarrier&, MTL::Fence*);
        void parkRelease(const RHIBufferBarrier&, MTL::Fence*);
        MTL::Fence* parkReleases(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>,
            RHIBarrierSync& syncBeforeUnion
        );
    };
}
