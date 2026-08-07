#pragma once

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    // helper for resource barriers: most of barriers come from a usage pair.
    enum class RHIResourceUsage: u8{
        // first use / discard; only valid as the before side of an acquire!
        Undefined,
        // swapchain only
        Present,

        // buffer only
        VertexBuffer, IndexBuffer, UniformBuffer, IndirectArgs,

        // texture only
        RenderTarget, DepthWrite, DepthRead,

        // buffer/texture (structured buffer SRV/UAV included)
        SampledVertex, SampledFragment, SampledCompute,
        StorageCompute, CopySrc, CopyDst,
    };

    namespace detail{
        inline constexpr RHIBarrierPoint Expand(RHIResourceUsage usage){
            using enum RHIResourceUsage;
            using S = RHIBarrierSync;
            using A = RHIBarrierAccess;
            using L = RHITextureLayout;

            switch(usage){
            // special states - Sync::None <=> Access::NoAccess pairing
            case Undefined:       return {S::None,            A::NoAccess,        L::Undefined};
            case Present:         return {S::None,            A::NoAccess,        L::Present};

            // buffer only - layout is a dummy
            case VertexBuffer:    return {S::VertexShading,   A::VertexBuffer,    L::Undefined};
            case IndexBuffer:     return {S::IndexInput,      A::IndexBuffer,     L::Undefined};
            case UniformBuffer:   return {S::AllShading,      A::UniformBuffer,   L::Undefined};
            case IndirectArgs:    return {S::ExecuteIndirect, A::IndirectArgs,    L::Undefined};

            // render pass
            case RenderTarget:    return {S::RenderTarget,    A::RenderTarget,    L::RenderTarget};
            case DepthWrite:      return {S::DepthStencil,    A::DepthWrite,      L::DepthWrite};
            case DepthRead:       return {S::DepthStencil,    A::DepthRead,       L::DepthRead};

            // shader read - same access/layout, only the sync differs
            case SampledVertex:   return {S::VertexShading,   A::ShaderResource,  L::ShaderResource};
            case SampledFragment: return {S::PixelShading,    A::ShaderResource,  L::ShaderResource};
            case SampledCompute:  return {S::Compute,         A::ShaderResource,  L::ShaderResource};

            // shader write
            case StorageCompute:  return {S::Compute,         A::UnorderedAccess, L::UnorderedAccess};

            // transfer
            case CopySrc:         return {S::Copy,            A::CopySrc,         L::CopySrc};
            case CopyDst:         return {S::Copy,            A::CopyDst,         L::CopyDst};
            }

            // no default label: -Wswitch forces a triple for every new usage
            return {S::None, A::NoAccess, L::Undefined};
        }

        // which producer stages each pass kind can release
        inline constexpr auto RENDER_RELEASE_SYNC = combine(
            RHIBarrierSync::All,
            RHIBarrierSync::Draw,
            RHIBarrierSync::IndexInput,
            RHIBarrierSync::VertexShading,
            RHIBarrierSync::PixelShading,
            RHIBarrierSync::DepthStencil,
            RHIBarrierSync::RenderTarget,
            RHIBarrierSync::ExecuteIndirect
        );
        inline constexpr auto COMPUTE_RELEASE_SYNC = combine(
            RHIBarrierSync::All,
            RHIBarrierSync::Compute
        );
        inline constexpr auto COPY_RELEASE_SYNC = combine(
            RHIBarrierSync::All,
            RHIBarrierSync::Copy
        );

        [[maybe_unused]]
        inline constexpr bool syncMatchesPass(RHIBarrierSync sync, RHIBarrierSync allowed){
            return (static_cast<u32>(sync) & ~static_cast<u32>(allowed)) == 0;
        }

        // Sync::None demands Access::NoAccess. the reverse is allowed:
        // NoAccess under real sync is an execution-only dependency
        // (e.g. cross-submission discard — nothing to flush, only ordering)
        [[maybe_unused]]
        inline constexpr bool halfIsPaired(RHIBarrierSync sync, RHIBarrierAccess access){
            return sync != RHIBarrierSync::None ||
                access == RHIBarrierAccess::NoAccess;
        }

        // write accesses cannot combine with anything
        [[maybe_unused]]
        inline constexpr bool writeAccessAlone(RHIBarrierAccess access){
            constexpr auto writes = combine(
                RHIBarrierAccess::RenderTarget,
                RHIBarrierAccess::DepthWrite,
                RHIBarrierAccess::UnorderedAccess,
                RHIBarrierAccess::CopyDst
            );
            if(!hasFlag(access, writes)){
                return true;
            }

            const auto bits = static_cast<u32>(access);
            return (bits & (bits - 1)) == 0;
        }

        inline void validateEdge(const RHITextureBarrier& barrier){
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
                barrier.discard == (barrier.layoutBefore == RHITextureLayout::Undefined),
                "discard ⇔ layoutBefore == Undefined"
            );
            CROWY_ASSERT(barrier.layoutAfter != RHITextureLayout::Undefined,
                "Undefined layout is only valid on the before side"
            );
        }

        inline void validateEdge(const RHIBufferBarrier& barrier){
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
        inline void validateAcquire(const Barrier& barrier){
            validateEdge(barrier);
            CROWY_ASSERT(
                !barrier.crossSubmission ||
                barrier.syncBefore != RHIBarrierSync::None,
                "a cross-submission acquire names the earlier submission's "
                "real work; without one it is a plain self-contained acquire"
            );
        }

        template<typename Barrier>
        void validateRelease(const Barrier& barrier, RHIBarrierSync allowedSync){
            validateEdge(barrier);
            CROWY_ASSERT(barrier.syncBefore != RHIBarrierSync::None,
                "a release needs real producer work; "
                "first use is a self-contained acquire instead"
            );
            CROWY_ASSERT(syncMatchesPass(barrier.syncBefore, allowedSync),
                "release syncBefore does not match this pass kind"
            );
            CROWY_ASSERT(!barrier.crossSubmission,
                "crossSubmission marks acquires; a release's producer is this pass"
            );
        }

        inline void validateAcquires(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ){
            for(const auto& acquire: textureAcquires){
                validateAcquire(acquire);
            }
            for(const auto& acquire: bufferAcquires){
                validateAcquire(acquire);
            }
        }

        inline void validateReleases(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases,
            RHIBarrierSync allowedSync
        ){
            for(const auto& release: textureReleases){
                validateRelease(release, allowedSync);
            }
            for(const auto& release: bufferReleases){
                validateRelease(release, allowedSync);
            }
        }
    }

    inline constexpr bool IsBufferOnlyUsage(RHIResourceUsage u){
        return u == RHIResourceUsage::VertexBuffer  || u == RHIResourceUsage::IndexBuffer
            || u == RHIResourceUsage::UniformBuffer || u == RHIResourceUsage::IndirectArgs;
    }
    inline constexpr bool IsTextureOnlyUsage(RHIResourceUsage u){
        return u == RHIResourceUsage::RenderTarget || u == RHIResourceUsage::DepthWrite
            || u == RHIResourceUsage::DepthRead    || u == RHIResourceUsage::Present;
    }

    inline constexpr RHITextureBarrier MakeBarrier(
        RHITexture& texture,
        RHIResourceUsage before,
        RHIResourceUsage after,
        RHISubresourceRange range = {}
    ){
        CROWY_ASSERT(after != RHIResourceUsage::Undefined,
            "Undefined is only valid as the before side of an acquire"
        );
        CROWY_ASSERT(!IsBufferOnlyUsage(before) && !IsBufferOnlyUsage(after));
        const auto b = detail::Expand(before);
        const auto a = detail::Expand(after);

        return RHITextureBarrier{
            .texture      = &texture,
            .syncBefore   = b.sync,   .syncAfter   = a.sync,
            .accessBefore = b.access, .accessAfter = a.access,
            .layoutBefore = b.layout, .layoutAfter = a.layout,
            .range        = range,
            .discard      = (before == RHIResourceUsage::Undefined)
        };
    }

    inline constexpr RHIBufferBarrier MakeBarrier(
        RHIBuffer& buffer,
        RHIResourceUsage before,
        RHIResourceUsage after
    ){
        CROWY_ASSERT(after != RHIResourceUsage::Undefined,
            "Undefined is only valid as the before side of an acquire"
        );
        CROWY_ASSERT(!IsTextureOnlyUsage(before) && !IsTextureOnlyUsage(after));
        const auto b = detail::Expand(before);
        const auto a = detail::Expand(after);

        return RHIBufferBarrier{
            .buffer       = &buffer,
            .syncBefore   = b.sync,   .syncAfter   = a.sync,
            .accessBefore = b.access, .accessAfter = a.access
        };
    }

    // acquire against work from an earlier submission (typically the previous frame):
    // there is no release in this command list to pair with,
    // but the sync applies to the whole queue timeline, so ordering still holds.
    // `before` names the earlier submission's last real use of the resource.
    // discardContents drops the old contents (execution-only dependency) -
    // the pass must not load them.
    inline constexpr RHITextureBarrier MakeCrossSubmissionBarrier(
        RHITexture& texture,
        RHIResourceUsage before,
        RHIResourceUsage after,
        bool discardContents = false,
        RHISubresourceRange range = {}
    ){
        CROWY_ASSERT(
            before != RHIResourceUsage::Undefined &&
            before != RHIResourceUsage::Present,
            "cross-submission needs the earlier submission's real work; "
            "use MakeBarrier with Undefined/Present instead"
        );
        auto barrier = MakeBarrier(texture, before, after, range);
        if(discardContents){
            barrier.accessBefore = RHIBarrierAccess::NoAccess;
            barrier.layoutBefore = RHITextureLayout::Undefined;
            barrier.discard = true;
        }
        barrier.crossSubmission = true;

        return barrier;
    }

    inline constexpr RHIBufferBarrier MakeCrossSubmissionBarrier(
        RHIBuffer& buffer,
        RHIResourceUsage before,
        RHIResourceUsage after
    ){
        CROWY_ASSERT(
            before != RHIResourceUsage::Undefined &&
            before != RHIResourceUsage::Present,
            "cross-submission needs the earlier submission's real work; "
            "use MakeBarrier with Undefined instead"
        );
        auto barrier = MakeBarrier(buffer, before, after);
        barrier.crossSubmission = true;

        return barrier;
    }

    static_assert(detail::Expand(RHIResourceUsage::Undefined).access
        == RHIBarrierAccess::NoAccess);
    // no layout transition between Sampled* usages
    static_assert(detail::Expand(RHIResourceUsage::SampledFragment).layout
        == detail::Expand(RHIResourceUsage::SampledCompute).layout);

    class RHICommandList{
    private:
    #if defined(_DEBUG) || !defined(NDEBUG)
        enum class PassKind: u8{
            None, Render, Compute, Blit
        } passState = PassKind::None;
    #endif

    public:
        CROWY_DECLARE_INTERFACE(RHICommandList)

        // Command list lifecycle
        virtual void Begin(){
            CROWY_ASSERT(passState == PassKind::None,
                "Begin inside a pass. Did you call the matching End*Pass()?"
            );
        }
        virtual void Close(){
            CROWY_ASSERT(passState == PassKind::None,
                "Close inside a pass. Did you call the matching End*Pass()?"
            );
        }

        // Render Pass control
        virtual void BeginRenderPass(
            const RHIRenderPassDesc& desc,
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ){
            CROWY_ASSERT(passState == PassKind::None,
                "Already inside a pass. Did you call the matching End*Pass()?"
            );
            CROWY_ASSERT(desc.colorAttachments.size() > 0);
            detail::validateAcquires(textureAcquires, bufferAcquires);

        #if defined(_DEBUG) || !defined(NDEBUG)
            passState = PassKind::Render;
        #endif
        }
        virtual void EndRenderPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
            detail::validateReleases(
                textureReleases,
                bufferReleases,
                detail::RENDER_RELEASE_SYNC
            );

        #if defined(_DEBUG) || !defined(NDEBUG)
            passState = PassKind::None;
        #endif
        }

        // Pipeline state
        virtual void SetPipelineState(RHIGraphicsPipelineState&){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }

        // Vertex and index buffers
        // stride = sizeof(Vertex)
        virtual void SetVertexBuffer(
            RHIBuffer&,
            u32 slot,
            u32 stride,
            u32 offset = 0
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }

        virtual void SetIndexBuffer(
            RHIBuffer&,
            RHIIndexFormat format = RHIIndexFormat::UInt32,
            u32 offset = 0
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }

        virtual void SetPushGraphicsConstants(
            const void* data,
            u32 size
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(size % 4 == 0 && size <= RHI_PUSH_CONSTANT_BYTES);
        }

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushGraphicsConstants(const T& t){
            SetPushGraphicsConstants(&t, sizeof(T));
        }

        virtual void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(offset < buffer.GetSize());

            CROWY_ASSERT(slot < RHI_NUM_DIRECT_CBS);
            CROWY_ASSERT(offset % RHI_CB_ALIGN == 0);
        }

        // Viewport and scissor
        virtual void SetViewport(const RHIViewport&){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }
        virtual void SetScissorRect(const RHIScissorRect&){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }

        // Draw commands
        virtual void Draw(
            u32 vertexCount,
            u32 instanceCount = 1,
            u32 startVertex = 0,
            u32 startInstance = 0
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }

        virtual void DrawIndexed(
            u32 indexCount,
            u32 instanceCount = 1,
            u32 startIndex = 0,
            i32 baseVertex = 0,
            u32 startInstance = 0
        ){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );
        }

        // binds batch.pso, then issues batch.drawCount
        // indirect draws from batch.args (RHIDrawIndexedArgs[])
        virtual void ExecuteIndirect(const DrawBatch& batch){
            CROWY_ASSERT(passState == PassKind::Render,
                "Not in a render pass. Did you call RHICommandList::BeginRenderPass()?"
            );

            CROWY_ASSERT(batch.pso != nullptr);
            CROWY_ASSERT(batch.args != nullptr);
            CROWY_ASSERT(batch.countBuffer == nullptr,
                "countBuffer is reserved for GPU-driven compaction"
            );
        }

        // Compute Pass control
        virtual void BeginComputePass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ){
            CROWY_ASSERT(passState == PassKind::None,
                "Already inside a pass. Did you call the matching End*Pass()?"
            );
            detail::validateAcquires(textureAcquires, bufferAcquires);

        #if defined(_DEBUG) || !defined(NDEBUG)
            passState = PassKind::Compute;
        #endif
        }
        virtual void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ){
            CROWY_ASSERT(passState == PassKind::Compute,
                "Not in a compute pass. Did you call RHICommandList::BeginComputePass()?"
            );
            detail::validateReleases(
                textureReleases,
                bufferReleases,
                detail::COMPUTE_RELEASE_SYNC
            );

        #if defined(_DEBUG) || !defined(NDEBUG)
            passState = PassKind::None;
        #endif
        }

        virtual void SetPipelineState(RHIComputePipelineState&){
            CROWY_ASSERT(passState == PassKind::Compute,
                "Not in a compute pass. Did you call RHICommandList::BeginComputePass()?"
            );
        }

        virtual void SetPushComputeConstants(
            const void* data,
            u32 size
        ){
            CROWY_ASSERT(passState == PassKind::Compute,
                "Not in a compute pass. Did you call RHICommandList::BeginComputePass()?"
            );

            CROWY_ASSERT(size % 4 == 0 && size <= RHI_PUSH_CONSTANT_BYTES);
        }

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushComputeConstants(const T& t){
            SetPushComputeConstants(&t, sizeof(T));
        }

        virtual void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ){
            CROWY_ASSERT(passState == PassKind::Compute,
                "Not in a compute pass. Did you call RHICommandList::BeginComputePass()?"
            );

            CROWY_ASSERT(offset < buffer.GetSize());

            CROWY_ASSERT(slot < RHI_NUM_DIRECT_CBS);
            CROWY_ASSERT(offset % RHI_CB_ALIGN == 0);
        }

        // Compute dispatch
        virtual void Dispatch(
            Size3D gridSize
        ){
            CROWY_ASSERT(passState == PassKind::Compute,
                "Not in a compute pass. Did you call RHICommandList::BeginComputePass()?"
            );
        }

        // hazards between dispatches *inside* one compute pass
        // (encoder-internal barrier); illegal anywhere else
        virtual void DispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers = {},
            std::span<const RHIBufferBarrier> bufferBarriers = {}
        ){
            CROWY_ASSERT(passState == PassKind::Compute,
                "DispatchBarrier is only legal inside a compute pass"
            );

            for([[maybe_unused]] const auto& barrier: textureBarriers){
                CROWY_ASSERT(
                    barrier.syncBefore == RHIBarrierSync::Compute &&
                    barrier.syncAfter == RHIBarrierSync::Compute &&
                    barrier.accessBefore == RHIBarrierAccess::UnorderedAccess &&
                    barrier.accessAfter == RHIBarrierAccess::UnorderedAccess &&
                    barrier.layoutBefore == RHITextureLayout::UnorderedAccess &&
                    barrier.layoutAfter == RHITextureLayout::UnorderedAccess,
                    "DispatchBarrier only expresses UAV → UAV hazards; "
                    "anything else is a pass boundary"
                );
            }
            for([[maybe_unused]] const auto& barrier: bufferBarriers){
                CROWY_ASSERT(
                    barrier.syncBefore == RHIBarrierSync::Compute &&
                    barrier.syncAfter == RHIBarrierSync::Compute &&
                    barrier.accessBefore == RHIBarrierAccess::UnorderedAccess &&
                    barrier.accessAfter == RHIBarrierAccess::UnorderedAccess,
                    "DispatchBarrier only expresses UAV → UAV hazards; "
                    "anything else is a pass boundary"
                );
            }
        }

        // Blit Pass control
        virtual void BeginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ){
            CROWY_ASSERT(passState == PassKind::None,
                "Already inside a pass. Did you call the matching End*Pass()?"
            );
            detail::validateAcquires(textureAcquires, bufferAcquires);

        #if defined(_DEBUG) || !defined(NDEBUG)
            passState = PassKind::Blit;
        #endif
        }
        virtual void EndBlitPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ){
            CROWY_ASSERT(passState == PassKind::Blit,
                "Not in a blit pass. Did you call RHICommandList::BeginBlitPass()?"
            );
            detail::validateReleases(
                textureReleases,
                bufferReleases,
                detail::COPY_RELEASE_SYNC
            );

        #if defined(_DEBUG) || !defined(NDEBUG)
            passState = PassKind::None;
        #endif
        }

        // Copy operations
        virtual void Copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            usize srcOffset,
            usize dstOffset,
            usize size
        ){
            CROWY_ASSERT(passState == PassKind::Blit,
                "Not in a blit pass. Did you call RHICommandList::BeginBlitPass()?"
            );
        }

        virtual void Copy(
            RHITexture& src,
            RHITexture& dst
        ){
            CROWY_ASSERT(passState == PassKind::Blit,
                "Not in a blit pass. Did you call RHICommandList::BeginBlitPass()?"
            );
        }

        // helper for RHISwapchain(backBuffer)
        void Copy(
            RHITexture& src,
            RHISwapchain& dst
        ){
            auto& backBuffer = dst.GetCurrentTexture();
            Copy(
                src,
                backBuffer
            );
        }

        virtual void Copy(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ){
            CROWY_ASSERT(passState == PassKind::Blit,
                "Not in a blit pass. Did you call RHICommandList::BeginBlitPass()?"
            );
        }

        void Copy(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ){
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

        // virtual void WaitUntilCompleted() = 0;

        // Debug markers (for GPU profiling)
        virtual void BeginEvent(CStr name) = 0;
        virtual void EndEvent() = 0;
        virtual void SetMarker(CStr name) = 0;

        // // for UI,
        // //   DeviceContext for D3D11,
        // //   CommandBuffer for Metal,
        // //   CommandList for D3D12
        // virtual void* GetNative() noexcept = 0;
    };
}
