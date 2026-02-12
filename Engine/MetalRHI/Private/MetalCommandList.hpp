#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include "assert.hpp"
#include "MetalBuffer.hpp"
#include "MetalFence.hpp"
#include "MetalPipelineState.hpp"
#include "MetalSampler.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif

namespace Crowy
{
    inline auto convert(RHILoadAction action){
        switch(action){
        case RHILoadAction::Load:     return MTL::LoadActionLoad;
        case RHILoadAction::Clear:    return MTL::LoadActionClear;
        case RHILoadAction::DontCare: return MTL::LoadActionDontCare;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHIStoreAction action){
        switch(action){
        case RHIStoreAction::Store:    return MTL::StoreActionStore;
        case RHIStoreAction::DontCare: return MTL::StoreActionDontCare;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHICullMode mode){
        switch(mode){
        case RHICullMode::CullNone: return MTL::CullModeNone;
        case RHICullMode::Front:    return MTL::CullModeFront;
        case RHICullMode::Back:     return MTL::CullModeBack;
        default:
            std::unreachable();
        }
    }

    inline auto convert(RHIPrimitiveTopology topology){
        switch(topology){
        case RHIPrimitiveTopology::PointList:     return MTL::PrimitiveTypePoint;
        case RHIPrimitiveTopology::LineList:      return MTL::PrimitiveTypeLine;
        case RHIPrimitiveTopology::LineStrip:     return MTL::PrimitiveTypeLineStrip;
        case RHIPrimitiveTopology::TriangleList:  return MTL::PrimitiveTypeTriangle;
        case RHIPrimitiveTopology::TriangleStrip: return MTL::PrimitiveTypeTriangleStrip;
        default:
            std::unreachable();
        }
    }

    class MetalCommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
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
        uint32_t currentIndexBufferOffset = 0;
        MTL::IndexType currentIndexFormat = MTL::IndexTypeUInt32;
        
        RHIPrimitiveTopology currentTopology = RHIPrimitiveTopology::TriangleList;
        bool isRecording = false;

    public:
        MetalCommandList(
            MTL::CommandQueue* queue
        ) noexcept
            : commandQueue(queue)
        {}
        ~MetalCommandList(){
            reset();
        }

        void begin() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(!isRecording,
                "Did you call RHICommandList::close()?"
            );

            commandBuffer = commandQueue->commandBuffer();
            commandBuffer->setLabel(
                NS::String::string("Crowy Command Buffer", NS::UTF8StringEncoding)
            );
            isRecording = true;
        }

        void flush() noexcept RHI_OVERRIDE{
            if(renderEncoder){
                renderEncoder->endEncoding();
                renderEncoder = nullptr;
            }
            else if(computeEncoder){
                computeEncoder->endEncoding();
                computeEncoder = nullptr;
            }
            else if(blitEncoder){
                blitEncoder->endEncoding();
                blitEncoder = nullptr;
            }
        }

        void close() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );

            CROWY_ASSERT(renderEncoder == nullptr,
                "Did you call RHICommandList::endRenderPass()?"
            );
            CROWY_ASSERT(computeEncoder == nullptr,
                "Did you call RHICommandList::endCompute()?"
            );
            if(blitEncoder != nullptr){
                blitEncoder->endEncoding();
                blitEncoder = nullptr;
            }

            isRecording = false;
        }

        void reset() noexcept RHI_OVERRIDE{
            if(isRecording){
                flush();

                isRecording = false;
            }
            else{
                CROWY_ASSERT( renderEncoder == nullptr);
                CROWY_ASSERT(computeEncoder == nullptr);
                CROWY_ASSERT(   blitEncoder == nullptr);
            }
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
            CROWY_ASSERT(renderTargets.size() > 0);

            std::vector<const MTL::Texture*> texes;
            texes.reserve(renderTargets.size());

            for(const auto& renderTarget: renderTargets){
                auto tex = static_cast<MetalTexture*>(renderTarget)->get();

                CROWY_ASSERT(tex != nullptr);
                texes.push_back(tex);
            }

            beginRenderPass(
                texes,
                depthStencil,
                loadAction, storeAction,
                clearColor, clearDS,
                debugName
            );
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
            auto& mtlSwapchain = static_cast<MetalSwapchain&>(swapchain);
            std::array<const MTL::Texture*, 1> renderTarget{
                mtlSwapchain.getCurrentTexture()
            };

            beginRenderPass(
                renderTarget,
                depthStencil,
                loadAction, storeAction,
                clearColor, clearDS,
                debugName
            );
        }

        void endRenderPass() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            renderEncoder->endEncoding();
            renderEncoder = nullptr;
        }

        void setPipelineState(RHIPipelineState* pso) noexcept RHI_OVERRIDE{
            auto metalPSO = static_cast<MetalPipelineState*>(pso);
            currentTopology = metalPSO->getTopology();

            if(metalPSO->isComputePipeline()){
                CROWY_ASSERT(computeEncoder != nullptr,
                    "Did you call RHICommandList::beginCompute()?"
                );

                computeEncoder->setComputePipelineState(
                    metalPSO->getComputePipeline()
                );
            }
            else{
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setRenderPipelineState(
                    metalPSO->getRenderPipeline()
                );
                if(auto ds = metalPSO->getDepthStencilState()){
                    renderEncoder->setDepthStencilState(ds);
                }

                // Rasterizer state
                const auto& raster = metalPSO->getRasterizerState();
                renderEncoder->setCullMode(convert(raster.cullMode));
                renderEncoder->setFrontFacingWinding(
                    raster.frontCounterClockwise ? 
                        MTL::WindingCounterClockwise : 
                        MTL::WindingClockwise
                );
                renderEncoder->setTriangleFillMode(
                    raster.fillMode == RHIFillMode::Wireframe ?
                        MTL::TriangleFillModeLines :
                        MTL::TriangleFillModeFill
                );
                renderEncoder->setDepthBias(
                    raster.depthBias,
                    raster.slopeScaledDepthBias,
                    raster.depthBiasClamp
                );
                renderEncoder->setDepthClipMode(
                    raster.depthClipEnable ?
                        MTL::DepthClipModeClip :
                        MTL::DepthClipModeClamp
                );
            }
        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            auto mtlBuffer = static_cast<MetalBuffer&>(buffer).get();
            renderEncoder->setVertexBuffer(mtlBuffer, offset, slot);
        }

        void setIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            currentIndexBuffer = static_cast<MetalBuffer&>(buffer).get();
            currentIndexBufferOffset = offset;
            currentIndexFormat = (format == RHIIndexFormat::UInt16) ?
                MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
        }

        void setConstantBuffer(
            RHIShaderStage stage,
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto mtlBuffer = static_cast<MetalBuffer&>(buffer).get();

            switch(stage){
            case RHIShaderStage::VertexShader:
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setVertexBuffer(mtlBuffer, offset, slot);
                break;
            case RHIShaderStage::FragmentShader:
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setFragmentBuffer(mtlBuffer, offset, slot);
                break;
            case RHIShaderStage::ComputeShader:
                CROWY_ASSERT(computeEncoder != nullptr,
                    "Did you call RHICommandList::beginCompute()?"
                );
                computeEncoder->setBuffer(mtlBuffer, offset, slot);
                break;
            default:
                std::unreachable();
            }
        }

        void setTexture(
            uint32_t slot,
            RHITexture& texture,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            auto mtlTexture = static_cast<MetalTexture&>(texture).get();

            switch(stage){
            case RHIShaderStage::VertexShader:
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setVertexTexture(mtlTexture, slot);
                break;
            case RHIShaderStage::FragmentShader:
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setFragmentTexture(mtlTexture, slot);
                break;
            case RHIShaderStage::ComputeShader:
                CROWY_ASSERT(computeEncoder != nullptr,
                    "Did you call RHICommandList::beginCompute()?"
                );
                computeEncoder->setTexture(mtlTexture, slot);
                break;
            default:
                std::unreachable();
            }
        }

        void setBuffer(
            uint32_t slot,
            RHIBuffer& buffer,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            auto mtlBuffer = static_cast<MetalBuffer&>(buffer).get();

            if(stage == RHIShaderStage::ComputeShader){
                CROWY_ASSERT(computeEncoder != nullptr,
                    "Did you call RHICommandList::beginCompute()?"
                );
                computeEncoder->setBuffer(mtlBuffer, 0, slot);
            }
        }

        void setSampler(
            uint32_t slot,
            RHISampler& sampler,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            auto mtlSampler = static_cast<MetalSampler&>(sampler).get();

            switch(stage){
            case RHIShaderStage::VertexShader:
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setVertexSamplerState(mtlSampler, slot);
                break;
            case RHIShaderStage::FragmentShader:
                CROWY_ASSERT(renderEncoder != nullptr,
                    "Did you call RHICommandList::beginRenderPass()?"
                );
                renderEncoder->setFragmentSamplerState(mtlSampler, slot);
                break;
            case RHIShaderStage::ComputeShader:
                CROWY_ASSERT(computeEncoder != nullptr,
                    "Did you call RHICommandList::beginCompute()?"
                );
                computeEncoder->setSamplerState(mtlSampler, slot);
                break;
            default:
                std::unreachable();
            }   
        }

        void setViewport(const RHIViewport& viewport) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            MTL::Viewport vp{
                viewport.x, viewport.y,
                viewport.width, viewport.height,
                viewport.minDepth, viewport.maxDepth
            };
            renderEncoder->setViewport(vp);
        }

        void setScissorRect(const RHIScissorRect& scissor) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            MTL::ScissorRect rect{
                static_cast<NS::UInteger>(scissor.left),
                static_cast<NS::UInteger>(scissor.top),
                static_cast<NS::UInteger>(scissor.right - scissor.left),
                static_cast<NS::UInteger>(scissor.bottom - scissor.top)
            };
            renderEncoder->setScissorRect(rect);
        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );

            renderEncoder->drawPrimitives(
                convert(currentTopology),
                startVertex,
                vertexCount,
                instanceCount,
                startInstance
            );
        }

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            CROWY_ASSERT(currentIndexBuffer != nullptr);

            auto indexSize = (currentIndexFormat == MTL::IndexTypeUInt16) ? 2 : 4;
            auto indexOffset = currentIndexBufferOffset + startIndex * indexSize;

            renderEncoder->drawIndexedPrimitives(
                convert(currentTopology),
                indexCount,
                currentIndexFormat,
                currentIndexBuffer,
                indexOffset,
                instanceCount,
                baseVertex,
                startInstance
            );
        }

        void beginCompute() noexcept{
            CROWY_ASSERT(computeEncoder == nullptr,
                "Did you call RHICommandList::endCompute()?"
            );
            CROWY_ASSERT(renderEncoder == nullptr);
            if(blitEncoder != nullptr){
                blitEncoder->endEncoding();
                blitEncoder = nullptr;
            }

            computeEncoder = commandBuffer->computeCommandEncoder();
        }

        void endCompute() noexcept{
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );

            computeEncoder->endEncoding();
            computeEncoder = nullptr;
        }

        void dispatch(
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY,
            uint32_t threadGroupCountZ
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );

            // TODO: need proper threadgroup size
            MTL::Size threadgroupSize = MTL::Size::Make(16, 16, 1);
            MTL::Size gridSize = MTL::Size::Make(
                threadGroupCountX, threadGroupCountY, threadGroupCountZ
            );

            computeEncoder->dispatchThreadgroups(gridSize, threadgroupSize);
        }

        void transitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{
            // Metal has implicit synchronization between render passes,
            // so we only need to track state for API consistency.
            // memoryBarrier is only needed for same-pass synchronization.
            texture.setState(after);
        }

        void transitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{
            // no-op for Metal
            buffer.setState(after);
        }

        void uavBarrier(RHITexture&) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->memoryBarrier(MTL::BarrierScopeTextures);
        }

        void uavBarrier(RHIBuffer&) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->memoryBarrier(MTL::BarrierScopeBuffers);
        }

        void signalFence(RHIFence& fence, uint64_t value) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(commandBuffer != nullptr,
                "Did you call RHICommandList::begin()?"
            );

            auto& metalFence = static_cast<MetalFence&>(fence);
            commandBuffer->encodeSignalEvent(
                metalFence.getSharedEvent(), value
            );
        }

        void waitFence(RHIFence& fence, uint64_t value) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(commandBuffer != nullptr,
                "Did you call RHICommandList::begin()?"
            );

            auto& metalFence = static_cast<MetalFence&>(fence);
            commandBuffer->encodeWait(
                metalFence.getSharedEvent(), value
            );
        }

        void copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) noexcept RHI_OVERRIDE{
            ensureBlitEncoder();

            auto srcBuf = static_cast<MetalBuffer&>(src).get();
            auto dstBuf = static_cast<MetalBuffer&>(dst).get();

            blitEncoder->copyFromBuffer(
                srcBuf, srcOffset,
                dstBuf, dstOffset,
                size
            );
        }

        void copy(
            RHITexture& src,
            RHITexture& dst
        ) noexcept RHI_OVERRIDE{
            ensureBlitEncoder();

            auto srcTex = static_cast<MetalTexture&>(src).get();
            auto dstTex = static_cast<MetalTexture&>(dst).get();

            blitEncoder->copyFromTexture(srcTex, dstTex);
        }

        void copy(
            RHITexture& src,
            RHISwapchain& swapchain
        ) noexcept RHI_OVERRIDE{
            ensureBlitEncoder();

            auto& mtlSwapchain = static_cast<MetalSwapchain&>(swapchain);

            auto srcTex = static_cast<MetalTexture&>(src).get();
            auto dstTex = mtlSwapchain.getCurrentTexture();

            blitEncoder->copyFromTexture(srcTex, dstTex);
        }

        void copy(
            RHIBuffer& src,
            RHITexture& dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            ensureBlitEncoder();

            auto srcBuf = static_cast<MetalBuffer&>(src).get();
            auto dstTex = static_cast<MetalTexture&>(dst).get();

            auto width = dstTex->width();
            auto height = dstTex->height();
            auto bytesPerRow = width * getBytesPerPixel(dst.getFormat());
            auto bytesPerImage = bytesPerRow * height;

            blitEncoder->copyFromBuffer(
                srcBuf, 0,
                bytesPerRow, bytesPerImage,
                MTL::Size::Make(width, height, 1),
                dstTex, arraySlice, mipLevel,
                MTL::Origin::Make(0, 0, 0)
            );
        }

        void beginEvent(const char* name) noexcept RHI_OVERRIDE{
            auto str = NS::String::string(name, NS::UTF8StringEncoding);
            if(renderEncoder){
                renderEncoder->pushDebugGroup(str);
            }
            else if(computeEncoder){
                computeEncoder->pushDebugGroup(str);
            }
            else if(blitEncoder){
                blitEncoder->pushDebugGroup(str);
            }
        }

        void endEvent() noexcept RHI_OVERRIDE{
            if(renderEncoder){
                renderEncoder->popDebugGroup();
            }
            else if(computeEncoder){
                computeEncoder->popDebugGroup();
            }
            else if(blitEncoder){
                blitEncoder->popDebugGroup();
            }
        }

        void setMarker(const char* name) noexcept RHI_OVERRIDE{
            auto str = NS::String::string(name, NS::UTF8StringEncoding);
            if(renderEncoder){
                renderEncoder->insertDebugSignpost(str);
            }
            else if(computeEncoder){
                computeEncoder->insertDebugSignpost(str);
            }
            else if(blitEncoder){
                blitEncoder->insertDebugSignpost(str);
            }
        }

        void* getNative() const noexcept RHI_OVERRIDE{
            return commandBuffer;
        }

        auto get() const noexcept{ return commandBuffer; }

    private:
        void beginRenderPass(
            std::span<const MTL::Texture*> texes,
            // MTL::Texture* tex,
            RHITexture* depthStencil,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept{
            CROWY_ASSERT(renderEncoder == nullptr,
                "Did you call RHICommandList::endRenderPass()?"
            );
            CROWY_ASSERT(computeEncoder == nullptr);
            if(blitEncoder != nullptr){
                blitEncoder->endEncoding();
                blitEncoder = nullptr;
            }

            auto passDesc = MTL::RenderPassDescriptor::alloc()->init();

            // Color Attachment
            for(size_t i=0; i<texes.size(); ++i){
                auto& colorAttach = *passDesc->colorAttachments()->object(i);
                colorAttach.setTexture(texes[i]);
                colorAttach.setLoadAction(convert(loadAction));
                colorAttach.setStoreAction(convert(storeAction));
                colorAttach.setClearColor(MTL::ClearColor::Make(
                    clearColor.r, clearColor.g, clearColor.b, clearColor.a
                ));
            }

            // Depth Attachment
            if(depthStencil){
                auto depthTex = static_cast<MTL::Texture*>(
                    static_cast<MetalTexture&>(*depthStencil).get()
                );
                auto& depthAttach = *passDesc->depthAttachment();
                depthAttach.setTexture(depthTex);
                depthAttach.setLoadAction(convert(loadAction));
                depthAttach.setStoreAction(convert(storeAction));
                depthAttach.setClearDepth(clearDS.depth);
            }

            renderEncoder = commandBuffer->renderCommandEncoder(passDesc);
            if(debugName != nullptr){
                renderEncoder->setLabel(
                    NS::String::string(debugName, NS::UTF8StringEncoding)
                );
            }

            passDesc->release();
        }

        void ensureBlitEncoder() noexcept{
            CROWY_ASSERT(renderEncoder == nullptr,
                "Did you call RHICommandList::endRenderPass()?"
            );
            CROWY_ASSERT(computeEncoder == nullptr,
                "Did you call RHICommandList::endCompute()?"
            );

            // for reuse blit encoder
            if(blitEncoder == nullptr){
                blitEncoder = commandBuffer->blitCommandEncoder();
            }
        }
    };
}
