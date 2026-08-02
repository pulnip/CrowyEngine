#pragma once

#include <variant>
#include <Metal/MTLCommandBuffer.hpp>
#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLStageInputOutputDescriptor.hpp>
#include <Metal/MTLTypes.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include "RHIAPI.hpp"
#include "RHICommandList.hpp"

namespace Crowy
{
    class MetalGraphicsPipelineState;

    class MetalCommandList final: public RHICommandList{
    private:
        struct RenderPassState{
            MTL::RenderCommandEncoder* encoder = nullptr;

            MetalGraphicsPipelineState* pso = nullptr;
            MTL::PrimitiveType topology = MTL::PrimitiveType::PrimitiveTypeTriangle;

            MTL::Buffer* indexBuffer = nullptr;
            u32 indexBufferOffset = 0;
            MTL::IndexType indexFormat = MTL::IndexTypeUInt32;
        };

        struct ComputePassState{
            MTL::ComputeCommandEncoder* encoder = nullptr;

            MTL::Size threadsPerThreadgroup = {0, 0, 0};
        };

        struct BlitPassState{
            MTL::BlitCommandEncoder* encoder = nullptr;
        };

    private:
        MTL::CommandQueue* commandQueue = nullptr;
        // Queue-shared fence for explicit barriers
        MTL::Fence* barrier = nullptr;
        NS::SharedPtr<MTL::CommandBuffer> commandBuffer;

        std::variant<
            RenderPassState,
            ComputePassState,
            BlitPassState,
            std::monostate
        > passState = std::monostate{};

        CA::MetalDrawable* currentDrawable = nullptr;

        bool isRecording = false;
        // set by TransitionBarrier between passes;
        // consumed by the next encoder as waitForFence
        bool barrierPending = false;

    public:
        MetalCommandList(
            MTL::CommandQueue*,
            MTL::Fence*
        );
        ~MetalCommandList();

        void Begin() RHI_OVERRIDE;
        void Close() RHI_OVERRIDE;

        void BeginRenderPass(const RHIRenderPassDesc&) RHI_OVERRIDE;
        void EndRenderPass() RHI_OVERRIDE;

        void SetPipelineState(RHIGraphicsPipelineState& pso) RHI_OVERRIDE;

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

        void SetPushGraphicsConstants(
            const void* data,
            u32 size
        ) RHI_OVERRIDE;

        void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
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
            int32_t baseVertex = 0,
            u32 startInstance = 0
        ) RHI_OVERRIDE;

        void ExecuteIndirect(const DrawBatch&) RHI_OVERRIDE;

        void BeginCompute() RHI_OVERRIDE;
        void EndCompute() RHI_OVERRIDE;

        void SetPipelineState(RHIComputePipelineState& pso) RHI_OVERRIDE;

        void SetPushComputeConstants(
            const void* data,
            u32 size
        ) RHI_OVERRIDE;

        void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void Dispatch(Size3D gridSize) RHI_OVERRIDE;

        void BeginBlit() RHI_OVERRIDE;
        void EndBlit() RHI_OVERRIDE;

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

        void TransitionBarrier(
            std::span<const RHITextureBarrier>,
            std::span<const RHIBufferBarrier>
        ) RHI_OVERRIDE;

        void WaitUntilCompleted();

        void BeginEvent(CStr name) RHI_OVERRIDE;
        void EndEvent() RHI_OVERRIDE;

        void SetMarker(CStr name) RHI_OVERRIDE;

        auto Get() const noexcept{ return commandBuffer.get(); }
    };
}
