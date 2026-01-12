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

        void begin() RHI_OVERRIDE{

        }

        void close() RHI_OVERRIDE{

        }

        void reset() RHI_OVERRIDE{

        }

        void beginRenderPass(
            RHITexture* renderTarget,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) RHI_OVERRIDE{

        }

        void beginRenderPass(
            RHISwapchain* swapchain,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) RHI_OVERRIDE{

        }

        void endRenderPass() RHI_OVERRIDE{

        }

        void setPipelineState(RHIPipelineState* pso) RHI_OVERRIDE{

        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) RHI_OVERRIDE{

        }

        void setIndexBuffer(
            RHIBuffer* buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) RHI_OVERRIDE{

        }

        void setConstantBuffer(
            RHIShaderStage stage,
            uint32_t slot,
            RHIBuffer* buffer,
            uint32_t offset = 0
        ) RHI_OVERRIDE{

        }

        void setTexture(
            uint32_t slot,
            RHITexture* texture,
            RHIShaderStage stage
        ) RHI_OVERRIDE{

        }

        void setBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            RHIShaderStage stage
        ) RHI_OVERRIDE{

        }

        void setViewport(const RHIViewport& viewport) RHI_OVERRIDE{

        }

        void setScissorRect(const RHIScissorRect& scissor) RHI_OVERRIDE{

        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) RHI_OVERRIDE{

        }

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) RHI_OVERRIDE{

        }

        void dispatch(
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY,
            uint32_t threadGroupCountZ
        ) RHI_OVERRIDE{

        }

        void transitionBarrier(
            RHITexture* texture,
            RHIResourceState before,
            RHIResourceState after
        ) RHI_OVERRIDE{

        }

        void transitionBarrier(
            RHIBuffer* buffer,
            RHIResourceState before,
            RHIResourceState after
        ) RHI_OVERRIDE{

        }

        void uavBarrier(RHITexture*) RHI_OVERRIDE{

        }

        void uavBarrier(RHIBuffer*) RHI_OVERRIDE{

        }

        void signalFence(RHIFence*, uint64_t value) RHI_OVERRIDE{

        }

        void waitFence(RHIFence*, uint64_t value) RHI_OVERRIDE{

        }

        void copyBuffer(
            RHIBuffer* src,
            RHIBuffer* dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) RHI_OVERRIDE{

        }

        void copyTexture(
            RHITexture* src,
            RHITexture* dst
        ) RHI_OVERRIDE{

        }

        void copyBufferToTexture(
            RHIBuffer* src,
            RHITexture* dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) RHI_OVERRIDE{

        }

        void beginEvent(const char* name) RHI_OVERRIDE{

        }

        void endEvent() RHI_OVERRIDE{

        }

        void setMarker(const char* name) RHI_OVERRIDE{

        }
    };
}
