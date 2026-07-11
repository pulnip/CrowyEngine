#pragma once

#include "RHIFWD.hpp"
#include "Semantics.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // Command list for recording GPU commands
    class RHICommandList{
    public:
        SMOL_DECLARE_INTERFACE(RHICommandList)

        // Command list lifecycle
        virtual void Begin() = 0;
        virtual void Flush() = 0;
        virtual void Close() = 0;
        virtual void Reset() = 0;

        // Render pass control
        virtual void BeginRenderPass(const RHIRenderPassDesc&) = 0;
        virtual void EndRenderPass() = 0;

        // Pipeline state
        virtual void SetPipelineState(RHIGraphicsPipelineState&) = 0;
        virtual void SetPipelineState(RHIComputePipelineState&) = 0;

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

        // for Data shared within Multiple Draw Call
        virtual void SetConstantBuffer(
            RHIBuffer&,
            u32 slot,
            RHIShaderStage,
            u32 offset = 0
        ) = 0;

        // Shader resources (textures, buffers)
        virtual void SetTexture(
            RHITexture&,
            u32 slot,
            RHIBindingAccess,
            RHIShaderStage
        ) = 0;

        // only for Compute Shader
        virtual void SetBuffer(
            RHIBuffer&,
            u32 slot,
            RHIBindingAccess,
            RHIShaderStage stage = RHIShaderStage::ComputeShader
        ) = 0;

        // for per-draw data, size should be <= 256B
        template<typename T>
            requires (!std::is_pointer_v<T> && std::is_trivially_copyable_v<T>)
        void SetBytes(
            const T& data,
            u32 slot,
            RHIShaderStage stage
        ){
            // type-safe helper
            SetBytes(&data, sizeof(T), slot, stage);
        }

        virtual void SetSampler(
            RHISampler&,
            u32 slot,
            RHIShaderStage
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

        // Compute dispatch
        virtual void Dispatch(
            Size3D gridSize
        ) = 0;

        // Resource barriers (state transitions)
        // Note: 'before' state is obtained from texture.getState() internally
        virtual void TransitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) = 0;

        virtual void TransitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
        ) = 0;

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

        virtual void Copy(
            RHITexture& src,
            RHISwapchain& dst
        ) = 0;

        virtual void Copy(
            RHIBuffer& src,
            RHITexture& dst,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) = 0;

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

    private:
        // direct use of void* is unsafe
        virtual void SetBytes(
            const void* bytes,
            usize size,
            u32 slot,
            RHIShaderStage stage
        ) = 0;
    };
}
