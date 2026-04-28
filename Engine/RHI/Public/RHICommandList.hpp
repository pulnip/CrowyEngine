#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include "RHIFWD.hpp"
#include "semantics.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalCommandList.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11CommandList.hpp"
    #else
        #include "NullCommandList.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHICommandListType = requires(T cmdList,
    ){
    };
    static_assert(RHICommandListType<RHICommandList>);
#else
    // Command list for recording GPU commands
    class RHICommandList{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHICommandList)

        // Command list lifecycle
        virtual void begin() noexcept = 0;
        virtual void flush() noexcept = 0;
        virtual void close() noexcept = 0;
        virtual void reset() noexcept = 0;

        // Render pass control
        virtual void beginRenderPass(
            std::span<RHITexture*> renderTargets,
            RHITexture* depthTarget = nullptr,
            RHILoadAction loadAction  = RHILoadAction::Load,
            RHIStoreAction storeAction = RHIStoreAction::Store,
            const RHIClearColor& clearColor = {},
            const RHIClearDepthStencil& clearDS = {},
            const char* debugName = nullptr
        ) noexcept = 0;

        virtual void beginRenderPass(
            RHISwapchain& backBuffer,
            RHITexture* depthTarget = nullptr,
            RHILoadAction loadAction  = RHILoadAction::Load,
            RHIStoreAction storeAction = RHIStoreAction::Store,
            const RHIClearColor& clearColor = {},
            const RHIClearDepthStencil& clearDS = {},
            const char* debugName = nullptr
        ) noexcept = 0;

        virtual void endRenderPass() noexcept = 0;

        // Pipeline state
        virtual void setPipelineState(RHIGraphicsPipelineState&) noexcept = 0;
        virtual void setPipelineState(RHIComputePipelineState&) noexcept = 0;

        // Vertex and index buffers
        virtual void setVertexBuffer(
            const RHIBuffer&,
            uint32_t slot,
            uint32_t stride = sizeof(Vertex),
            uint32_t offset = 0
        ) noexcept = 0;

        virtual void setIndexBuffer(
            const RHIBuffer&,
            RHIIndexFormat format = RHIIndexFormat::UInt32,
            uint32_t offset = 0
        ) noexcept = 0;

        // Constant buffers
        virtual void setConstantBuffer(
            const RHIBuffer&,
            uint32_t slot,
            RHIShaderStage,
            uint32_t offset = 0
        ) noexcept = 0;

        // Shader resources (textures, buffers)
        virtual void setTexture(
            RHITexture&,
            uint32_t slot,
            RHIBindingAccess,
            RHIShaderStage
        ) noexcept = 0;

        // only for Compute Shader
        virtual void setBuffer(
            RHIBuffer&,
            uint32_t slot,
            RHIBindingAccess,
            RHIShaderStage stage = RHIShaderStage::ComputeShader
        ) = 0;

        virtual void setBytes(
            const void* bytes,
            uint32_t slot,
            size_t size,
            RHIShaderStage stage = RHIShaderStage::ComputeShader
        ) = 0;

        virtual void setSampler(
            const RHISampler&,
            uint32_t slot,
            RHIShaderStage
        ) noexcept = 0;

        // Viewport and scissor
        virtual void setViewport(const RHIViewport&) noexcept = 0;
        virtual void setScissorRect(const RHIScissorRect&) noexcept = 0;

        // Draw commands
        virtual void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) noexcept = 0;

        virtual void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) noexcept = 0;

        virtual void beginCompute() noexcept = 0;

        virtual void endCompute() noexcept = 0;

        // Compute dispatch
        virtual void dispatch(
            RHISize3D gridSize
        ) noexcept = 0;

        // Resource barriers (state transitions)
        // Note: 'before' state is obtained from texture.getState() internally
        virtual void transitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) noexcept = 0;

        virtual void transitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
        ) noexcept = 0;

        virtual void uavBarrier(RHITexture&) noexcept = 0;
        virtual void uavBarrier(RHIBuffer&) noexcept = 0;

        virtual void signalFence(RHIFence&, uint64_t value) noexcept = 0;
        virtual void waitFence(RHIFence&, uint64_t value) noexcept = 0;

        // Copy operations
        virtual void copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) noexcept = 0;

        virtual void copy(
            RHITexture& src,
            RHITexture& dst
        ) noexcept = 0;

        virtual void copy(
            RHITexture& src,
            RHISwapchain& dst
        ) noexcept = 0;

        virtual void copy(
            RHIBuffer& src,
            RHITexture& dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) noexcept = 0;

        virtual void waitUntilCompleted() noexcept = 0;

        // Debug markers (for GPU profiling)
        virtual void beginEvent(const char* name) noexcept = 0;
        virtual void endEvent() noexcept = 0;
        virtual void setMarker(const char* name) noexcept = 0;

        // for UI, CommandBuffer for Metal, CommandList for D3D12
        virtual void* getNative() const noexcept{ return nullptr; }
    };
#endif

    using RHICommandListPtr = std::unique_ptr<RHICommandList>;
}
