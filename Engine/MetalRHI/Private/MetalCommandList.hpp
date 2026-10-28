#pragma once

#include <array>
#include <optional>
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

#include "MetalDefinitions.hpp"
#include "MetalPipelineState.hpp"
#include "MetalUtil.hpp"
#include "RHICommandList.hpp"
#include "RHIReleaseLedger.hpp"

namespace Crowy
{
    class MetalCommandList final: public RHICommandList {
    private:
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

        RefPtr<MTL::CommandBuffer> commandBuffer;

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
        std::vector<RefPtr<MTL::Fence>> fencePool;
        usize fenceCursor = 0;

        // each fence is updated by its producer's encoder and waited on by
        // every acquire (several waits on one fence are legal); an entry no
        // acquire matched is a cross-submission hand-off the device gates on
        RHIReleaseLedger<MTL::Fence*> releases;

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

        auto Get() const noexcept { return commandBuffer.get(); }

        // a parked release no acquire matched: its consumers are in later
        // submissions, which the device gates while this wave may run
        bool HasUnconsumedReleases() const noexcept;

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

        void setGraphicsPipelineState(RHIGraphicsPipelineState&) override;

        void setPushGraphicsConstants(const void* data, u32 size) override;

        void setGraphicsConstantBuffer(
            RHIBuffer&,
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
            RHIBuffer&,
            u32 slot,
            u32 offset
        ) override;

        void dispatch(Size3D gridSize) override;

        void dispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers,
            std::span<const RHIBufferBarrier> bufferBarriers
        ) override;

        void beginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) override;
        void endBlitPass(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
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

        MTL::Fence* acquireFence();

        // encode the lazy submission-event wait (once per recording);
        // legal only between encoders
        void encodeSubmissionWait();

        // Begin*Pass halves: one wait per distinct producer fence, so a
        // non-adjacent A-B-C dependency picks A's fence; entries stay parked
        void gatherWaits(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>
        );

        // End*Pass halves: park real edges under one fresh fence the caller
        // updates; self-contained releases (syncAfter None) record nothing
        MTL::Fence* parkReleases(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>,
            RHIBarrierSync& syncBeforeUnion
        );
    };
}
