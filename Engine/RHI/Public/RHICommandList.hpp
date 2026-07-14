#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // helper for resource barrier
    enum class RHIResourceUsage: u16{
        // swapchain
        Present,
        // buffer
        VertexBuffer,
        IndexBuffer,
        ConstantBuffer,
        IndirectArgument,
        // texture
        RenderTarget,
        DepthStencilWrite,
        DepthStencilRead,
        // buffer/texture
        AnyRead,
        VertexRead,
        FragmentRead,
        ComputeRead,
        ComputeWrite,
        CopySrc,
        CopyDst,
    };

    namespace detail{
        struct RHIBarrierPoint{
            RHIBarrierSync sync;
            RHIBarrierAccess access;
            RHIBarrierLayout layout;
        };

        constexpr RHIBarrierPoint Expand(RHIResourceUsage usage){
            using enum RHIResourceUsage;
            using S = RHIBarrierSync;
            using A = RHIBarrierAccess;
            using L = RHIBarrierLayout;

            switch(usage){
            case Present:
                return {S::None, A::NoAccess, L::Present};
            // buffer
            case VertexBuffer:
                return {S::Vertex, A::VertexBuffer, L::Undefined};
            case IndexBuffer:
                return {S::IndexInput, A::IndexBuffer, L::Undefined};
            case ConstantBuffer:
                return {S::AllShading, A::ConstantBuffer, L::Undefined};
            case IndirectArgument:
                return {S::ExecuteIndirect, A::IndirectArgument, L::Undefined};
            // texture in render pass
            case RenderTarget:
                return {S::RenderTarget, A::RenderTarget, L::RenderTarget};
            case DepthStencilWrite:
                return {S::DepthStencil, A::DepthStencilWrite, L::DepthStencilWrite};
            case DepthStencilRead:
                return {S::DepthStencil, A::DepthStencilRead, L::DepthStencilRead};
            // shader read/write
            case AnyRead:
                return {S::AllShading, A::ShaderResource, L::ShaderResource};
            case VertexRead:
                return {S::Vertex, A::ShaderResource, L::ShaderResource};
            case FragmentRead:
                return {S::Fragment, A::ShaderResource, L::ShaderResource};
            case ComputeRead:
                return {S::Compute, A::ShaderResource, L::ShaderResource};
            case ComputeWrite:
                return {S::Compute, A::UnorderedAccess, L::UnorderedAccess};
            // transfer
            case CopySrc:
                return {S::Copy, A::CopySrc, L::CopySrc};
            case CopyDst:
                return {S::Copy, A::CopyDst, L::CopyDst};
            }

            // error
            return {S::None, A::NoAccess, L::Undefined};
        }
    }

    // Command list for recording GPU commands
    class RHICommandList{
    public:
        CROWY_DECLARE_INTERFACE(RHICommandList)

        // Command list lifecycle
        virtual void Begin() = 0;
        virtual void Close() = 0;

        // Render pass control
        virtual void BeginRenderPass(const RHIRenderPassDesc&) = 0;
        virtual void EndRenderPass() = 0;

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

        virtual void BeginCompute() = 0;
        virtual void EndCompute() = 0;

        virtual void SetPipelineState(RHIComputePipelineState&) = 0;

        virtual void SetPushComputeConstants(
            const void* data,
            u32 size
        ) = 0;

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushComputeConstants(const T& t){
            setPushComputeConstants(&t, sizeof(T));
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

        virtual void BeginBlit() = 0;
        virtual void EndBlit() = 0;

        // Copy operations
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
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) = 0;

        // Resource barriers
        virtual void TransitionBarrier(
            RHITexture& texture,
            RHIBarrierSync syncAfter,
            RHIBarrierAccess accessAfter,
            RHIBarrierLayout layoutAfter
        ) = 0;
        virtual void TransitionBarrier(
            RHIBuffer& buffer,
            RHIBarrierSync syncAfter,
            RHIBarrierAccess accessAfter
        ) = 0;
        void TransitionBarrier(
            RHITexture& texture,
            RHIResourceUsage usageAfter
        ){
            const auto point = detail::Expand(usageAfter);

            TransitionBarrier(
                texture,
                point.sync,
                point.access,
                point.layout
            );
        }
        void TransitionBarrier(
            RHIBuffer& buffer,
            RHIResourceUsage usageAfter
        ){
            const auto point = detail::Expand(usageAfter);

            TransitionBarrier(
                buffer,
                point.sync,
                point.access
            );
        }

        virtual void WaitUntilCompleted() = 0;

        // Debug markers (for GPU profiling)
        virtual void BeginEvent(CStr name) = 0;
        virtual void EndEvent() = 0;
        virtual void SetMarker(CStr name) = 0;

        // for UI,
        //   DeviceContext for D3D11,
        //   CommandBuffer for Metal,
        //   CommandList for D3D12
        virtual void* GetNative() noexcept = 0;
    };
}
