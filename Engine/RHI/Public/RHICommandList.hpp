#pragma once

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RHIFrameStats.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    enum class RHIResourceUsage : u8 {
        // first use / discard; only valid as the before side of an acquire!
        Undefined,
        // swapchain only
        Present,

        // buffer only
        VertexBuffer,
        IndexBuffer,
        UniformBuffer,
        IndirectArgs,

        // texture only
        RenderTarget,
        DepthWrite,
        DepthRead,

        // buffer/texture (structured buffer SRV/UAV included)
        SampledVertex,
        SampledFragment,
        SampledCompute,
        StorageCompute,
        CopySrc,
        CopyDst,
    };

    namespace detail
    {
        inline constexpr RHIBarrierPoint expand(RHIResourceUsage usage) {
            using enum RHIResourceUsage;
            using S = RHIBarrierSync;
            using A = RHIBarrierAccess;
            using L = RHITextureLayout;

            switch(usage) {
            // special states - Sync::None <=> Access::NoAccess pairing
            case Undefined:
                return {S::None, A::NoAccess, L::Undefined};
            case Present:
                return {S::None, A::NoAccess, L::Present};

            // buffer only - layout is a dummy
            case VertexBuffer:
                return {S::VertexShading, A::VertexBuffer, L::Undefined};
            case IndexBuffer:
                return {S::IndexInput, A::IndexBuffer, L::Undefined};
            case UniformBuffer:
                return {S::AllShading, A::UniformBuffer, L::Undefined};
            case IndirectArgs:
                return {S::ExecuteIndirect, A::IndirectArgs, L::Undefined};

            case RenderTarget:
                return {S::RenderTarget, A::RenderTarget, L::RenderTarget};
            case DepthWrite:
                return {S::DepthStencil, A::DepthWrite, L::DepthWrite};
            case DepthRead:
                return {S::DepthStencil, A::DepthRead, L::DepthRead};

            case SampledVertex:
                return {S::VertexShading, A::ShaderResource, L::ShaderResource};
            case SampledFragment:
                return {S::PixelShading, A::ShaderResource, L::ShaderResource};
            case SampledCompute:
                return {S::Compute, A::ShaderResource, L::ShaderResource};

            case StorageCompute:
                return {S::Compute, A::UnorderedAccess, L::UnorderedAccess};

            case CopySrc:
                return {S::Copy, A::CopySrc, L::CopySrc};
            case CopyDst:
                return {S::Copy, A::CopyDst, L::CopyDst};
            }

            // no default label: -Wswitch forces a triple for every new usage
            return {S::None, A::NoAccess, L::Undefined};
        }

        inline constexpr auto RenderReleaseSync = combine(
            RHIBarrierSync::All,
            RHIBarrierSync::Draw,
            RHIBarrierSync::IndexInput,
            RHIBarrierSync::VertexShading,
            RHIBarrierSync::PixelShading,
            RHIBarrierSync::DepthStencil,
            RHIBarrierSync::RenderTarget,
            RHIBarrierSync::ExecuteIndirect
        );
        inline constexpr auto ComputeReleaseSync =
            combine(RHIBarrierSync::All, RHIBarrierSync::Compute);
        inline constexpr auto CopyReleaseSync =
            combine(RHIBarrierSync::All, RHIBarrierSync::Copy);

        [[maybe_unused]]
        inline constexpr bool syncMatchesPass(
            RHIBarrierSync sync,
            RHIBarrierSync allowed
        ) {
            return (static_cast<u32>(sync) & ~static_cast<u32>(allowed)) == 0;
        }

        // Sync::None demands Access::NoAccess; NoAccess under real sync is an
        // execution-only dependency, like a cross-submission discard
        [[maybe_unused]]
        inline constexpr bool halfIsPaired(
            RHIBarrierSync sync,
            RHIBarrierAccess access
        ) {
            return sync != RHIBarrierSync::None ||
                   access == RHIBarrierAccess::NoAccess;
        }

        [[maybe_unused]]
        inline constexpr bool writeAccessAlone(RHIBarrierAccess access) {
            constexpr auto writes = combine(
                RHIBarrierAccess::RenderTarget,
                RHIBarrierAccess::DepthWrite,
                RHIBarrierAccess::UnorderedAccess,
                RHIBarrierAccess::CopyDst
            );
            if(!hasFlag(access, writes)) {
                return true;
            }

            const auto bits = static_cast<u32>(access);
            return (bits & (bits - 1)) == 0;
        }

        inline void validateEdge(const RHITextureBarrier& barrier) {
            CROWY_ASSERT(
                halfIsPaired(barrier.syncBefore, barrier.accessBefore) &&
                    halfIsPaired(barrier.syncAfter, barrier.accessAfter),
                "Sync::None demands Access::NoAccess on the same half"
            );
            CROWY_ASSERT(
                writeAccessAlone(barrier.accessBefore) &&
                    writeAccessAlone(barrier.accessAfter),
                "write access must be a single bit"
            );
            CROWY_ASSERT(
                barrier.discard ==
                    (barrier.layoutBefore == RHITextureLayout::Undefined),
                "discard ⇔ layoutBefore == Undefined"
            );
            CROWY_ASSERT(
                barrier.layoutAfter != RHITextureLayout::Undefined,
                "Undefined layout is only valid on the before side"
            );
        }

        inline void validateEdge(const RHIBufferBarrier& barrier) {
            CROWY_ASSERT(
                halfIsPaired(barrier.syncBefore, barrier.accessBefore) &&
                    halfIsPaired(barrier.syncAfter, barrier.accessAfter),
                "Sync::None demands Access::NoAccess on the same half"
            );
            CROWY_ASSERT(
                writeAccessAlone(barrier.accessBefore) &&
                    writeAccessAlone(barrier.accessAfter),
                "write access must be a single bit"
            );
        }

        template<typename Barrier>
        inline void validateAcquire(const Barrier& barrier) {
            validateEdge(barrier);
            CROWY_ASSERT(
                !barrier.crossSubmission ||
                    barrier.syncBefore != RHIBarrierSync::None,
                "a cross-submission acquire names the earlier submission's "
                "real work; without one it is a plain self-contained acquire"
            );
        }

        template<typename Barrier>
        void validateRelease(
            const Barrier& barrier,
            RHIBarrierSync allowedSync
        ) {
            validateEdge(barrier);
            CROWY_ASSERT(
                barrier.syncBefore != RHIBarrierSync::None,
                "a release needs real producer work; "
                "first use is a self-contained acquire instead"
            );
            CROWY_ASSERT(
                syncMatchesPass(barrier.syncBefore, allowedSync),
                "release syncBefore does not match this pass kind"
            );
            CROWY_ASSERT(
                !barrier.crossSubmission,
                "crossSubmission marks acquires; a release's producer is this "
                "pass"
            );
        }

        inline void validateAcquires(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) {
            for(const auto& acquire: textureAcquires) {
                validateAcquire(acquire);
            }
            for(const auto& acquire: bufferAcquires) {
                validateAcquire(acquire);
            }
        }

        inline void validateReleases(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases,
            RHIBarrierSync allowedSync
        ) {
            for(const auto& release: textureReleases) {
                validateRelease(release, allowedSync);
            }
            for(const auto& release: bufferReleases) {
                validateRelease(release, allowedSync);
            }
        }

        inline void validateIndices(const RHIIndexBufferView& indices) {
            CROWY_ASSERT(
                indices.buffer != nullptr,
                "an indexed draw needs an index buffer in its "
                "RHIIndexBufferView"
            );
            CROWY_ASSERT(
                indices.offset % indexSize(indices.format) == 0,
                "index buffer offset must land on an index boundary"
            );
            CROWY_ASSERT(indices.offset < indices.buffer->GetSize());
        }
    }

    inline constexpr bool isBufferOnlyUsage(RHIResourceUsage u) {
        return u == RHIResourceUsage::VertexBuffer ||
               u == RHIResourceUsage::IndexBuffer ||
               u == RHIResourceUsage::UniformBuffer ||
               u == RHIResourceUsage::IndirectArgs;
    }
    inline constexpr bool isTextureOnlyUsage(RHIResourceUsage u) {
        return u == RHIResourceUsage::RenderTarget ||
               u == RHIResourceUsage::DepthWrite ||
               u == RHIResourceUsage::DepthRead ||
               u == RHIResourceUsage::Present;
    }

    inline constexpr RHITextureBarrier makeBarrier(
        RHITexture& texture,
        RHIResourceUsage before,
        RHIResourceUsage after,
        RHISubresourceRange range = {}
    ) {
        CROWY_ASSERT(
            after != RHIResourceUsage::Undefined,
            "Undefined is only valid as the before side of an acquire"
        );
        CROWY_ASSERT(!isBufferOnlyUsage(before) && !isBufferOnlyUsage(after));
        const auto b = detail::expand(before);
        const auto a = detail::expand(after);

        return RHITextureBarrier{
            .texture = &texture,
            .syncBefore = b.sync,
            .syncAfter = a.sync,
            .accessBefore = b.access,
            .accessAfter = a.access,
            .layoutBefore = b.layout,
            .layoutAfter = a.layout,
            .range = range,
            .discard = (before == RHIResourceUsage::Undefined)
        };
    }

    inline constexpr RHIBufferBarrier makeBarrier(
        RHIBuffer& buffer,
        RHIResourceUsage before,
        RHIResourceUsage after
    ) {
        CROWY_ASSERT(
            after != RHIResourceUsage::Undefined,
            "Undefined is only valid as the before side of an acquire"
        );
        CROWY_ASSERT(!isTextureOnlyUsage(before) && !isTextureOnlyUsage(after));
        const auto b = detail::expand(before);
        const auto a = detail::expand(after);

        return RHIBufferBarrier{
            .buffer = &buffer,
            .syncBefore = b.sync,
            .syncAfter = a.sync,
            .accessBefore = b.access,
            .accessAfter = a.access
        };
    }

    // an acquire against an earlier submission's last use `before`: no release
    // here pairs with it; with discardContents the pass must not load it
    inline constexpr RHITextureBarrier makeCrossSubmissionBarrier(
        RHITexture& texture,
        RHIResourceUsage before,
        RHIResourceUsage after,
        bool discardContents = false,
        RHISubresourceRange range = {}
    ) {
        CROWY_ASSERT(
            before != RHIResourceUsage::Undefined &&
                before != RHIResourceUsage::Present,
            "cross-submission needs the earlier submission's real work; "
            "use makeBarrier with Undefined/Present instead"
        );
        auto barrier = makeBarrier(texture, before, after, range);
        if(discardContents) {
            barrier.accessBefore = RHIBarrierAccess::NoAccess;
            barrier.layoutBefore = RHITextureLayout::Undefined;
            barrier.discard = true;
        }
        barrier.crossSubmission = true;

        return barrier;
    }

    inline constexpr RHIBufferBarrier makeCrossSubmissionBarrier(
        RHIBuffer& buffer,
        RHIResourceUsage before,
        RHIResourceUsage after
    ) {
        CROWY_ASSERT(
            before != RHIResourceUsage::Undefined &&
                before != RHIResourceUsage::Present,
            "cross-submission needs the earlier submission's real work; "
            "use makeBarrier with Undefined instead"
        );
        auto barrier = makeBarrier(buffer, before, after);
        barrier.crossSubmission = true;

        return barrier;
    }

    inline RHIViewport fullViewport(
        const RHITexture& texture,
        u32 mipLevel = 0
    ) {
        return RHIViewport{
            .x = 0,
            .y = 0,
            .width = static_cast<f32>(texture.GetWidth(mipLevel)),
            .height = static_cast<f32>(texture.GetHeight(mipLevel)),
            .minDepth = 0,
            .maxDepth = 1
        };
    }

    inline RHIScissorRect fullScissorRect(
        const RHITexture& texture,
        u32 mipLevel = 0
    ) {
        return RHIScissorRect{
            .left = 0,
            .top = 0,
            .right = static_cast<i32>(texture.GetWidth(mipLevel)),
            .bottom = static_cast<i32>(texture.GetHeight(mipLevel))
        };
    }

    static_assert(
        detail::expand(RHIResourceUsage::Undefined).access ==
        RHIBarrierAccess::NoAccess
    );
    static_assert(
        detail::expand(RHIResourceUsage::SampledFragment).layout ==
        detail::expand(RHIResourceUsage::SampledCompute).layout
    );

    class RHICommandList {
    private:
#if CROWY_DEBUG_BUILD
        enum class PassKind : u8 {
            None,
            Render,
            Compute,
            Blit
        } passState = PassKind::None;
#endif

#if CROWY_FRAME_STATS
        RHIFrameStats stats;
#endif

    public:
        CROWY_DECLARE_INTERFACE(RHICommandList)

#if CROWY_FRAME_STATS
        // what this command list recorded since its last Begin()
        const RHIFrameStats& GetStats() const noexcept { return stats; }
#else
        const RHIFrameStats& GetStats() const noexcept {
            static constexpr RHIFrameStats none;
            return none;
        }
#endif

        virtual void Begin() {
            CROWY_ASSERT(
                passState == PassKind::None,
                "Begin inside a pass. Did you call the matching End*Pass()?"
            );

#if CROWY_FRAME_STATS
            stats = RHIFrameStats{};
#endif
            CROWY_STAT(commandListBeginCount);
        }
        virtual void Close() {
            CROWY_ASSERT(
                passState == PassKind::None,
                "Close inside a pass. Did you call the matching End*Pass()?"
            );
        }

        virtual void BeginRenderPass(
            const RHIRenderPassDesc& desc,
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::None,
                "Already inside a pass. Did you call the matching End*Pass()?"
            );
            CROWY_ASSERT(
                !desc.colorAttachments.empty() ||
                    desc.depthAttachment.has_value(),
                "a render pass needs a color or a depth attachment"
            );
            detail::validateAcquires(textureAcquires, bufferAcquires);

            CROWY_STAT(renderPassCount);
            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureAcquires.size() + bufferAcquires.size()
            );

#if CROWY_DEBUG_BUILD
            passState = PassKind::Render;
#endif
        }
        virtual void EndRenderPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );
            detail::validateReleases(
                textureReleases,
                bufferReleases,
                detail::RenderReleaseSync
            );

            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureReleases.size() + bufferReleases.size()
            );

#if CROWY_DEBUG_BUILD
            passState = PassKind::None;
#endif
        }

        virtual void SetPipelineState(RHIGraphicsPipelineState&) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );

            CROWY_STAT(pipelineSetCount);
        }

        virtual void SetPushGraphicsConstants(const void* data, u32 size) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(size % 4 == 0 && size <= PushConstantBytes);

            CROWY_STAT(pushConstantSetCount);
        }

        template<typename T>
            requires(!std::is_pointer_v<T>)
        void SetPushGraphicsConstants(const T& t) {
            SetPushGraphicsConstants(&t, sizeof(T));
        }

        virtual void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(offset < buffer.GetSize());

            CROWY_ASSERT(slot < DirectCbCount);
            CROWY_ASSERT(offset % CbAlign == 0);

            CROWY_STAT(constantBufferSetCount);
        }

        void SetGraphicsConstantBuffer(const RHIBufferSlice& slice, u32 slot) {
            CROWY_ASSERT(slice.IsValid());

            SetGraphicsConstantBuffer(*slice.buffer, slot, slice.offset);
        }

        virtual void SetViewport(const RHIViewport&) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );
        }
        virtual void SetScissorRect(const RHIScissorRect&) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );
        }

        virtual void Draw(
            u32 vertexCount,
            u32 instanceCount = 1,
            u32 startVertex = 0,
            u32 startInstance = 0
        ) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );

            CROWY_STAT(drawCount);
        }

        virtual void DrawIndexed(
            const RHIIndexBufferView& indices,
            u32 indexCount,
            u32 instanceCount = 1,
            u32 startIndex = 0,
            i32 baseVertex = 0,
            u32 startInstance = 0
        ) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );
            detail::validateIndices(indices);

            CROWY_STAT(drawCount);
        }

        // binds batch.pso, then issues batch.drawCount
        // indirect draws from batch.args (RHIDrawArgs[])
        virtual void ExecuteIndirect(const DrawBatch& batch) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(batch.pso != nullptr);
            CROWY_ASSERT(batch.args != nullptr);

            CROWY_STAT(indirectBatchCount);
            CROWY_STAT_ADD(indirectDrawCount, batch.drawCount);
        }

        virtual void ExecuteIndirectIndexed(const DrawBatchIndexed& batch) {
            CROWY_ASSERT(
                passState == PassKind::Render,
                "Not in a render pass. Did you call "
                "RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(batch.pso != nullptr);
            CROWY_ASSERT(batch.args != nullptr);
            detail::validateIndices(batch.indices);

            CROWY_STAT(indirectBatchCount);
            CROWY_STAT_ADD(indirectDrawCount, batch.drawCount);
        }

        virtual void BeginComputePass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::None,
                "Already inside a pass. Did you call the matching End*Pass()?"
            );
            detail::validateAcquires(textureAcquires, bufferAcquires);

            CROWY_STAT(computePassCount);
            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureAcquires.size() + bufferAcquires.size()
            );

#if CROWY_DEBUG_BUILD
            passState = PassKind::Compute;
#endif
        }
        virtual void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::Compute,
                "Not in a compute pass. Did you call "
                "RHICommandList::BeginComputePass()?"
            );
            detail::validateReleases(
                textureReleases,
                bufferReleases,
                detail::ComputeReleaseSync
            );

            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureReleases.size() + bufferReleases.size()
            );

#if CROWY_DEBUG_BUILD
            passState = PassKind::None;
#endif
        }

        virtual void SetPipelineState(RHIComputePipelineState&) {
            CROWY_ASSERT(
                passState == PassKind::Compute,
                "Not in a compute pass. Did you call "
                "RHICommandList::BeginComputePass()?"
            );

            CROWY_STAT(pipelineSetCount);
        }

        virtual void SetPushComputeConstants(const void* data, u32 size) {
            CROWY_ASSERT(
                passState == PassKind::Compute,
                "Not in a compute pass. Did you call "
                "RHICommandList::BeginComputePass()?"
            );

            CROWY_ASSERT(size % 4 == 0 && size <= PushConstantBytes);

            CROWY_STAT(pushConstantSetCount);
        }

        template<typename T>
            requires(!std::is_pointer_v<T>)
        void SetPushComputeConstants(const T& t) {
            SetPushComputeConstants(&t, sizeof(T));
        }

        virtual void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) {
            CROWY_ASSERT(
                passState == PassKind::Compute,
                "Not in a compute pass. Did you call "
                "RHICommandList::BeginComputePass()?"
            );

            CROWY_ASSERT(offset < buffer.GetSize());

            CROWY_ASSERT(slot < DirectCbCount);
            CROWY_ASSERT(offset % CbAlign == 0);

            CROWY_STAT(constantBufferSetCount);
        }

        void SetComputeConstantBuffer(const RHIBufferSlice& slice, u32 slot) {
            CROWY_ASSERT(slice.IsValid());

            SetComputeConstantBuffer(*slice.buffer, slot, slice.offset);
        }

        virtual void Dispatch(Size3D gridSize) {
            CROWY_ASSERT(
                passState == PassKind::Compute,
                "Not in a compute pass. Did you call "
                "RHICommandList::BeginComputePass()?"
            );

            CROWY_STAT(dispatchCount);
        }

        // hazards between dispatches *inside* one compute pass
        // (encoder-internal barrier); illegal anywhere else
        virtual void DispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers = {},
            std::span<const RHIBufferBarrier> bufferBarriers = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::Compute,
                "DispatchBarrier is only legal inside a compute pass"
            );

            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureBarriers.size() + bufferBarriers.size()
            );

            for([[maybe_unused]] const auto& barrier: textureBarriers) {
                CROWY_ASSERT(
                    barrier.syncBefore == RHIBarrierSync::Compute &&
                        barrier.syncAfter == RHIBarrierSync::Compute &&
                        barrier.accessBefore ==
                            RHIBarrierAccess::UnorderedAccess &&
                        barrier.accessAfter ==
                            RHIBarrierAccess::UnorderedAccess &&
                        barrier.layoutBefore ==
                            RHITextureLayout::UnorderedAccess &&
                        barrier.layoutAfter ==
                            RHITextureLayout::UnorderedAccess,
                    "DispatchBarrier only expresses UAV → UAV hazards; "
                    "anything else is a pass boundary"
                );
            }
            for([[maybe_unused]] const auto& barrier: bufferBarriers) {
                CROWY_ASSERT(
                    barrier.syncBefore == RHIBarrierSync::Compute &&
                        barrier.syncAfter == RHIBarrierSync::Compute &&
                        barrier.accessBefore ==
                            RHIBarrierAccess::UnorderedAccess &&
                        barrier.accessAfter ==
                            RHIBarrierAccess::UnorderedAccess,
                    "DispatchBarrier only expresses UAV → UAV hazards; "
                    "anything else is a pass boundary"
                );
            }
        }

        virtual void BeginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::None,
                "Already inside a pass. Did you call the matching End*Pass()?"
            );
            detail::validateAcquires(textureAcquires, bufferAcquires);

            CROWY_STAT(blitPassCount);
            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureAcquires.size() + bufferAcquires.size()
            );

#if CROWY_DEBUG_BUILD
            passState = PassKind::Blit;
#endif
        }
        virtual void EndBlitPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) {
            CROWY_ASSERT(
                passState == PassKind::Blit,
                "Not in a blit pass. Did you call "
                "RHICommandList::BeginBlitPass()?"
            );
            detail::validateReleases(
                textureReleases,
                bufferReleases,
                detail::CopyReleaseSync
            );

            CROWY_STAT_ADD(
                barrierEdgeCount,
                textureReleases.size() + bufferReleases.size()
            );

#if CROWY_DEBUG_BUILD
            passState = PassKind::None;
#endif
        }

        virtual void Copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            usize srcOffset,
            usize dstOffset,
            usize size
        ) {
            CROWY_ASSERT(
                passState == PassKind::Blit,
                "Not in a blit pass. Did you call "
                "RHICommandList::BeginBlitPass()?"
            );

            CROWY_STAT(copyCount);
        }

        virtual void Copy(RHITexture& src, RHITexture& dst) {
            CROWY_ASSERT(
                passState == PassKind::Blit,
                "Not in a blit pass. Did you call "
                "RHICommandList::BeginBlitPass()?"
            );

            CROWY_STAT(copyCount);
        }

        void Copy(RHITexture& src, RHISwapchain& dst) {
            auto& backBuffer = dst.GetCurrentTexture();
            Copy(src, backBuffer);
        }

        virtual void Copy(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) {
            CROWY_ASSERT(
                passState == PassKind::Blit,
                "Not in a blit pass. Did you call "
                "RHICommandList::BeginBlitPass()?"
            );
            CROWY_ASSERT(
                dst.GetDepth(mipLevel) == 1,
                "RHITextureRegion cannot address a 3D texture's depth slices"
            );
            CROWY_ASSERT(
                region.x + region.width <= dst.GetWidth(mipLevel) &&
                    region.y + region.height <= dst.GetHeight(mipLevel),
                "copy region reaches past the mip"
            );

            CROWY_STAT(copyCount);
        }

        void Copy(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) {
            Copy(
                src,
                srcOffset,
                srcRowPitch,
                dst,
                RHITextureRegion{
                    .x = 0,
                    .y = 0,
                    .width = dst.GetWidth(mipLevel),
                    .height = dst.GetHeight(mipLevel)
                },
                mipLevel,
                arraySlice
            );
        }

        // the region's rows land dstRowPitch bytes apart, starting at dstOffset
        virtual void Copy(
            RHITexture& src,
            RHIBuffer& dst,
            u64 dstOffset,
            u32 dstRowPitch,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) {
            CROWY_ASSERT(
                passState == PassKind::Blit,
                "Not in a blit pass. Did you call "
                "RHICommandList::BeginBlitPass()?"
            );
            CROWY_ASSERT(
                src.GetDepth(mipLevel) == 1,
                "RHITextureRegion cannot address a 3D texture's depth slices"
            );
            CROWY_ASSERT(
                !isBlockCompressed(src.GetFormat()),
                "a block-compressed texture has no rows of texels to read back"
            );
            CROWY_ASSERT(
                region.width > 0 && region.height > 0,
                "an empty region copies nothing"
            );
            CROWY_ASSERT(
                region.x + region.width <= src.GetWidth(mipLevel) &&
                    region.y + region.height <= src.GetHeight(mipLevel),
                "copy region reaches past the mip"
            );
            CROWY_ASSERT(
                dstRowPitch >= getRowPitch(src.GetFormat(), region.width),
                "a row is wider than its pitch"
            );
            CROWY_ASSERT(
                dstOffset + u64{dstRowPitch} * (region.height - 1) +
                        getRowPitch(src.GetFormat(), region.width) <=
                    dst.GetSize(),
                "the last row reaches past the buffer"
            );

            CROWY_STAT(copyCount);
        }

        void Copy(
            RHITexture& src,
            RHIBuffer& dst,
            u64 dstOffset,
            u32 dstRowPitch,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) {
            Copy(
                src,
                dst,
                dstOffset,
                dstRowPitch,
                RHITextureRegion{
                    .x = 0,
                    .y = 0,
                    .width = src.GetWidth(mipLevel),
                    .height = src.GetHeight(mipLevel)
                },
                mipLevel,
                arraySlice
            );
        }

        virtual void BeginEvent(CStr name) = 0;
        virtual void EndEvent() = 0;
        virtual void SetMarker(CStr name) = 0;
    };
}
