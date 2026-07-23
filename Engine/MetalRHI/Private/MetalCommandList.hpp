#pragma once

#include <Metal/MTLCommandBuffer.hpp>
#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLBlitCommandEncoder.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLStageInputOutputDescriptor.hpp>
#include <Metal/MTLTypes.hpp>
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    class MetalCommandList final: public RHICommandList{
    private:
        MTL::CommandQueue* commandQueue = nullptr;
        MTL::CommandBuffer* commandBuffer = nullptr;
        // Begin-End of RenderEncoder should be called explicitly
        MTL::RenderCommandEncoder* renderEncoder = nullptr;
        // Begin-End of RenderEncoder should be called explicitly
        MTL::ComputeCommandEncoder* computeEncoder = nullptr;
        // implicitly reuse blit Encoder
        MTL::BlitCommandEncoder* blitEncoder = nullptr;

        CA::MetalDrawable* currentDrawable = nullptr;

        // Index buffer state
        MTL::Buffer* currentIndexBuffer = nullptr;
        u32 currentIndexBufferOffset = 0;
        MTL::IndexType currentIndexFormat = MTL::IndexTypeUInt32;

        MTL::PrimitiveType currentTopology = MTL::PrimitiveType::PrimitiveTypeTriangle;
        MTL::Size threadsPerThreadgroup = {0, 0, 0};
        bool isRecording = false;

    public:
        MetalCommandList(MTL::CommandQueue* queue);
        ~MetalCommandList();

        void Begin() RHI_OVERRIDE;
        void Flush() RHI_OVERRIDE;
        void Close() RHI_OVERRIDE;
        void Reset() RHI_OVERRIDE;

        void BeginRenderPass(const RHIRenderPassDesc&) RHI_OVERRIDE;
        void EndRenderPass() RHI_OVERRIDE;

        void SetPipelineState(RHIGraphicsPipelineState& pso) RHI_OVERRIDE;
        void SetPipelineState(RHIComputePipelineState& pso) RHI_OVERRIDE;

        void SetVertexBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 stride,
            u32 offset
        ) RHI_OVERRIDE;

        void SetIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            u32 offset
        ) RHI_OVERRIDE;

        void SetConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            RHIShaderStage stage,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void SetTexture(
            RHITexture& texture,
            u32 slot,
            RHIBindingAccess,
            RHIShaderStage stage
        ) RHI_OVERRIDE;

        void SetBuffer(
            RHIBuffer& buffer,
            u32 slot,
            RHIBindingAccess,
            RHIShaderStage stage
        ) RHI_OVERRIDE;

        void SetBytes(
            const void* bytes,
            usize size,
            u32 slot,
            RHIShaderStage stage
        ) RHI_OVERRIDE;

        void SetSampler(
            RHISampler& sampler,
            u32 slot,
            RHIShaderStage stage
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
            int32_t baseVertex = 0,
            u32 startInstance = 0
        ) RHI_OVERRIDE;

        void BeginCompute() RHI_OVERRIDE;
        void EndCompute() RHI_OVERRIDE;
        void Dispatch(Size3D gridSize) RHI_OVERRIDE;

        void TransitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) RHI_OVERRIDE;

        void TransitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
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
            RHITexture& src,
            RHISwapchain& swapchain
        ) RHI_OVERRIDE;

        void Copy(
            RHIBuffer& src,
            RHITexture& dst,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) RHI_OVERRIDE;

        void WaitUntilCompleted() RHI_OVERRIDE;

        void BeginEvent(CStr name) RHI_OVERRIDE;
        void EndEvent() RHI_OVERRIDE;

        void SetMarker(CStr name) RHI_OVERRIDE;

        void* GetNative() noexcept RHI_OVERRIDE{
            return commandBuffer;
        }

        auto Get() const noexcept{ return commandBuffer; }

    private:
        void ensureBlitEncoder();
    };
}
