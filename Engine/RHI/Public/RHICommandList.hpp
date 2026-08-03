#pragma once

#include "Assert.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "RHIDefinitions.hpp"
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
    public:
        CROWY_DECLARE_INTERFACE(RHICommandList)

        // Command list lifecycle
        virtual void Begin() = 0;
        virtual void Close() = 0;

        // Pass control
        virtual void BeginRenderPass(
            const RHIRenderPassDesc&,
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) = 0;
        virtual void EndRenderPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) = 0;

        virtual void BeginComputePass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) = 0;
        virtual void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) = 0;

        virtual void BeginCopyPass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) = 0;
        virtual void EndCopyPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) = 0;

        // hazards between dispatches *inside* one compute pass
        // (encoder-internal barrier); illegal anywhere else
        virtual void DispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers = {},
            std::span<const RHIBufferBarrier> bufferBarriers = {}
        ) = 0;

        // Pipeline state
        virtual void SetPipelineState(RHIGraphicsPipelineState&) = 0;

        // Vertex and index buffers
        // stride = sizeof(Vertex)
        virtual void SetVertexBuffer(
            RHIBuffer&,
            u32 slot,
            u32 stride,
            u32 offset = 0
        ) = 0;

        virtual void SetIndexBuffer(
            RHIBuffer&,
            RHIIndexFormat format = RHIIndexFormat::UInt32,
            u32 offset = 0
        ) = 0;

        virtual void SetPushGraphicsConstants(
            const void* data,
            u32 size
        ) = 0;

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushGraphicsConstants(const T& t){
            SetPushGraphicsConstants(&t, sizeof(T));
        }

        virtual void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) = 0;

        // Viewport and scissor
        virtual void SetViewport(const RHIViewport&) = 0;
        virtual void SetScissorRect(const RHIScissorRect&) = 0;

        // Draw commands
        virtual void Draw(
            u32 vertexCount,
            u32 instanceCount = 1,
            u32 startVertex = 0,
            u32 startInstance = 0
        ) = 0;

        virtual void DrawIndexed(
            u32 indexCount,
            u32 instanceCount = 1,
            u32 startIndex = 0,
            i32 baseVertex = 0,
            u32 startInstance = 0
        ) = 0;

        // binds batch.pso, then issues batch.drawCount
        // indirect draws from batch.args (RHIDrawIndexedArgs[])
        virtual void ExecuteIndirect(const DrawBatch& batch) = 0;

        virtual void SetPipelineState(RHIComputePipelineState&) = 0;

        virtual void SetPushComputeConstants(
            const void* data,
            u32 size
        ) = 0;

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushComputeConstants(const T& t){
            SetPushComputeConstants(&t, sizeof(T));
        }

        virtual void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) = 0;

        // Compute dispatch
        virtual void Dispatch(
            Size3D gridSize
        ) = 0;

        // Copy operations (legal inside a copy pass only)
        virtual void Copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            usize srcOffset,
            usize dstOffset,
            usize size
        ) = 0;

        virtual void Copy(
            RHITexture& src,
            RHITexture& dst
        ) = 0;

        // helper for RHISwapchain(backBuffer)
        void Copy(
            RHITexture& src,
            RHISwapchain& dst
        );

        virtual void Copy(
            RHIBuffer& src,
            u64 srcOffset,
            u32 srcRowPitch,
            RHITexture& dst,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) = 0;

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
