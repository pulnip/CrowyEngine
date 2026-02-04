#pragma once

#include <cstddef>
#include <memory>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif

namespace Crowy
{
    class NullCommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
    public:
        NullCommandList() = default;
        ~NullCommandList() = default;

        void begin() noexcept RHI_OVERRIDE{

        }

        void flush() noexcept RHI_OVERRIDE{

        }

        void close() noexcept RHI_OVERRIDE{

        }

        void reset() noexcept RHI_OVERRIDE{

        }

        void beginRenderPass(
            std::span<RHITexture*> renderTargets,
            RHITexture* depthStencil,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{

        }

        void beginRenderPass(
            RHISwapchain& swapchain,
            RHITexture* depthStencil,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{

        }

        void endRenderPass() noexcept RHI_OVERRIDE{

        }

        void setPipelineState(RHIPipelineState* pso) noexcept RHI_OVERRIDE{

        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{

        }

        void setIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{

        }

        void setConstantBuffer(
            RHIShaderStage stage,
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{

        }

        void setTexture(
            uint32_t slot,
            RHITexture& texture,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{

        }

        void setBuffer(
            uint32_t slot,
            RHIBuffer& buffer,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{

        }

        void setSampler(
            uint32_t slot,
            RHISampler& sampler,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{

        }

        void setViewport(const RHIViewport& viewport) noexcept RHI_OVERRIDE{

        }

        void setScissorRect(const RHIScissorRect& scissor) noexcept RHI_OVERRIDE{

        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{

        }

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{

        }

        void dispatch(
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY,
            uint32_t threadGroupCountZ
        ) noexcept RHI_OVERRIDE{

        }

        void transitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{

        }

        void transitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{

        }

        void uavBarrier(RHITexture&) noexcept RHI_OVERRIDE{

        }

        void uavBarrier(RHIBuffer&) noexcept RHI_OVERRIDE{

        }

        void signalFence(RHIFence&, uint64_t value) noexcept RHI_OVERRIDE{

        }

        void waitFence(RHIFence&, uint64_t value) noexcept RHI_OVERRIDE{

        }

        void copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) noexcept RHI_OVERRIDE{

        }

        void copy(
            RHITexture& src,
            RHITexture& dst
        ) noexcept RHI_OVERRIDE{

        }

        void copy(
            RHITexture& src,
            RHISwapchain& dst
        ) noexcept RHI_OVERRIDE{

        }

        void copy(
            RHIBuffer& src,
            RHITexture& dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{

        }

        void beginEvent(const char* name) noexcept RHI_OVERRIDE{

        }

        void endEvent() noexcept RHI_OVERRIDE{

        }

        void setMarker(const char* name) noexcept RHI_OVERRIDE{

        }
    };
}
