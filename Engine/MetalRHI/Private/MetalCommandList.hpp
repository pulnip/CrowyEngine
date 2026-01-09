#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include "MetalBuffer.hpp"
#include "MetalFence.hpp"
#include "MetalPipelineState.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif

namespace Crowy
{
    static MTL::LoadAction convertLoadAction(RHILoadStoreAction action){
        switch(action){
        case RHILoadStoreAction::Load:     return MTL::LoadActionLoad;
        case RHILoadStoreAction::Clear:    return MTL::LoadActionClear;
        case RHILoadStoreAction::DontCare: return MTL::LoadActionDontCare;
        default:
            std::unreachable();
        }
    }

    static MTL::StoreAction convertStoreAction(RHILoadStoreAction action){
        switch(action){
        case RHILoadStoreAction::Store:    return MTL::StoreActionStore;
        case RHILoadStoreAction::DontCare: return MTL::StoreActionDontCare;
        default:
            std::unreachable();
        }
    }

    static MTL::CullMode convertCullMode(RHICullMode mode){
        switch(mode){
        case RHICullMode::CullNone: return MTL::CullModeNone;
        case RHICullMode::Front:    return MTL::CullModeFront;
        case RHICullMode::Back:     return MTL::CullModeBack;
        default:
            std::unreachable();
        }
    }

    static MTL::PrimitiveType convertTopology(RHIPrimitiveTopology topology){
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
        MTL::RenderCommandEncoder* renderEncoder = nullptr;
        MTL::ComputeCommandEncoder* computeEncoder = nullptr;
        MTL::BlitCommandEncoder* blitEncoder = nullptr;
        MTL::SamplerState* defaultSampler = nullptr;

        CA::MetalDrawable* currentDrawable = nullptr;

        // Index buffer state
        MTL::Buffer* currentIndexBuffer = nullptr;
        uint32_t currentIndexBufferOffset = 0;
        MTL::IndexType currentIndexFormat = MTL::IndexTypeUInt32;
        
        RHIPrimitiveTopology currentTopology = RHIPrimitiveTopology::TriangleList;
        bool isRecording = false;

    public:
        MetalCommandList(
            MTL::CommandQueue* queue,
            MTL::SamplerState* defaultSampler = nullptr
        )
            : commandQueue(queue)
            , defaultSampler(defaultSampler)
        {}
        ~MetalCommandList(){
            reset();
        }

        void begin() RHI_OVERRIDE{
            if(isRecording) return;

            commandBuffer = commandQueue->commandBuffer();
            commandBuffer->setLabel(
                NS::String::string("Crowy Command Buffer", NS::UTF8StringEncoding)
            );
            isRecording = true;
        }

        void close() RHI_OVERRIDE{
            if(!isRecording) return;

            endCurrentEncoder();

            isRecording = false;
        }

        void reset() RHI_OVERRIDE{
            endCurrentEncoder();
            commandBuffer = nullptr;
            currentDrawable = nullptr;
            currentIndexBuffer = nullptr;
            isRecording = false;
        }

        void beginRenderPass(
            MTL::Texture* tex,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ){
            if(!isRecording || renderEncoder) return;

            auto passDesc = MTL::RenderPassDescriptor::alloc()->init();

            // Color Attachment
            auto colorAttach = passDesc->colorAttachments()->object(0);

            if(tex){
                colorAttach->setTexture(tex);
                colorAttach->setLoadAction(convertLoadAction(loadAction));
                colorAttach->setStoreAction(convertStoreAction(storeAction));
                colorAttach->setClearColor(MTL::ClearColor::Make(
                    clearColor.r, clearColor.g, clearColor.b, clearColor.a
                ));
            }

            // Depth Attachment
            if(depthStencil){
                auto depthTex = static_cast<MTL::Texture*>(
                    static_cast<MetalTexture*>(depthStencil)->get()
                );
                auto depthAttach = passDesc->depthAttachment();
                depthAttach->setTexture(depthTex);
                depthAttach->setLoadAction(convertLoadAction(loadAction));
                depthAttach->setStoreAction(convertStoreAction(storeAction));
                depthAttach->setClearDepth(clearDS.depth);
            }

            renderEncoder = commandBuffer->renderCommandEncoder(passDesc);
            renderEncoder->setLabel(
                NS::String::string("RenderToy Render Pass", NS::UTF8StringEncoding)
            );

            // 기본 sampler 설정
            if(defaultSampler){
                renderEncoder->setFragmentSamplerState(defaultSampler, 0);
            }

            passDesc->release();
        }

        void beginRenderPass(
            RHITexture* renderTarget,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) RHI_OVERRIDE{
            if(!renderTarget) return;

            MTL::Texture* tex = static_cast<MetalTexture*>(renderTarget)->get();

            beginRenderPass(
                tex,
                depthStencil,
                loadAction, storeAction,
                clearColor, clearDS
            );
        }

        void beginRenderPass(
            RHISwapchain* swapchain,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) RHI_OVERRIDE{
            if(!swapchain) return;

            auto mtlSwapchain = static_cast<MetalSwapchain*>(swapchain);

            beginRenderPass(
                mtlSwapchain->getCurrentDrawable()->texture(),
                depthStencil,
                loadAction, storeAction,
                clearColor, clearDS
            );
        }

        void endRenderPass() RHI_OVERRIDE{
            if(renderEncoder){
                renderEncoder->endEncoding();
                renderEncoder = nullptr;
            }
        }

        void setPipelineState(RHIPipelineState* pso) RHI_OVERRIDE{
            if(!pso) return;

            auto metalPSO = static_cast<MetalPipelineState*>(pso);
            currentTopology = metalPSO->getTopology();

            if(metalPSO->isComputePipeline()){
                endCurrentEncoder();

                if(!computeEncoder){
                    computeEncoder = commandBuffer->computeCommandEncoder();
                }
                computeEncoder->setComputePipelineState(
                    metalPSO->getComputePipeline()
                );
            }
            else{
                if(renderEncoder){
                    renderEncoder->setRenderPipelineState(
                        metalPSO->getRenderPipeline()
                    );

                    if(auto ds = metalPSO->getDepthStencilState()){
                        renderEncoder->setDepthStencilState(ds);
                    }

                    // Rasterizer state
                    const auto& raster = metalPSO->getRasterizerState();
                    renderEncoder->setCullMode(convertCullMode(raster.cullMode));
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
        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) RHI_OVERRIDE{
            if(!renderEncoder || !buffer) return;

            auto mtlBuffer = static_cast<MetalBuffer*>(buffer)->get();
            renderEncoder->setVertexBuffer(mtlBuffer, offset, slot);
        }

        void setIndexBuffer(
            RHIBuffer* buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) RHI_OVERRIDE{
            if(!buffer) return;

            currentIndexBuffer = static_cast<MetalBuffer*>(buffer)->get();
            currentIndexBufferOffset = offset;
            currentIndexFormat = (format == RHIIndexFormat::UInt16) ?
                MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
        }

        void setConstantBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            RHIShaderStage stage
        ) RHI_OVERRIDE{
            if(!buffer) return;

            auto mtlBuffer = static_cast<MetalBuffer*>(buffer)->get();

            if(renderEncoder){
                if(stage == RHIShaderStage::VertexShader){
                    renderEncoder->setVertexBuffer(mtlBuffer, 0, slot);
                }
                else if(stage == RHIShaderStage::FragmentShader){
                    renderEncoder->setFragmentBuffer(mtlBuffer, 0, slot);
                }
            }
            else if(computeEncoder && stage == RHIShaderStage::ComputeShader){
                computeEncoder->setBuffer(mtlBuffer, 0, slot);
            }
        }

        void setTexture(
            uint32_t slot,
            RHITexture* texture,
            RHIShaderStage stage
        ) RHI_OVERRIDE{
            if(!texture) return;

            auto mtlTexture = static_cast<MetalTexture*>(texture)->get();

            if(renderEncoder){
                if(stage == RHIShaderStage::VertexShader){
                    renderEncoder->setVertexTexture(mtlTexture, slot);
                }
                else if(stage == RHIShaderStage::FragmentShader){
                    renderEncoder->setFragmentTexture(mtlTexture, slot);
                }
            }
            else if(computeEncoder && stage == RHIShaderStage::ComputeShader){
                computeEncoder->setTexture(mtlTexture, slot);
            }
        }

        void setBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            RHIShaderStage stage
        ) RHI_OVERRIDE{
            if(!buffer) return;

            auto mtlBuffer = static_cast<MetalBuffer*>(buffer)->get();

            if(computeEncoder && stage == RHIShaderStage::ComputeShader){
                computeEncoder->setBuffer(mtlBuffer, 0, slot);
            }
        }

        void setSampler(
            MTL::SamplerState* sampler,
            uint32_t slot,
            RHIShaderStage stage
        ){
            if(!sampler) return;
            
            if(renderEncoder){
                if(stage == RHIShaderStage::VertexShader){
                    renderEncoder->setVertexSamplerState(sampler, slot);
                }
                else if(stage == RHIShaderStage::FragmentShader){
                    renderEncoder->setFragmentSamplerState(sampler, slot);
                }
            }
            else if(computeEncoder && stage == RHIShaderStage::ComputeShader){
                computeEncoder->setSamplerState(sampler, slot);
            }
        }

        void setViewport(const RHIViewport& viewport) RHI_OVERRIDE{
            if(!renderEncoder) return;

            MTL::Viewport vp{
                viewport.x, viewport.y,
                viewport.width, viewport.height,
                viewport.minDepth, viewport.maxDepth
            };
            renderEncoder->setViewport(vp);
        }

        void setScissorRect(const RHIScissorRect& scissor) RHI_OVERRIDE{
            if(!renderEncoder) return;

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
        ) RHI_OVERRIDE{
            if(!renderEncoder) return;

            renderEncoder->drawPrimitives(
                convertTopology(currentTopology),
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
        ) RHI_OVERRIDE{
            if(!renderEncoder || !currentIndexBuffer) return;

            auto indexSize = (currentIndexFormat == MTL::IndexTypeUInt16) ? 2 : 4;
            auto indexOffset = currentIndexBufferOffset + startIndex * indexSize;

            renderEncoder->drawIndexedPrimitives(
                convertTopology(currentTopology),
                indexCount,
                currentIndexFormat,
                currentIndexBuffer,
                indexOffset,
                instanceCount,
                baseVertex,
                startInstance
            );
        }

        void beginCompute(){
            if(!isRecording || computeEncoder) return;
            endCurrentEncoder();

            computeEncoder = commandBuffer->computeCommandEncoder();
        }

        void endCompute(){
            if(computeEncoder){
                computeEncoder->endEncoding();
                computeEncoder = nullptr;
            }
        }

        void dispatch(
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY,
            uint32_t threadGroupCountZ
        ) RHI_OVERRIDE{
            if(!computeEncoder) return;

            // TODO: need proper threadgroup size
            MTL::Size threadgroupSize = MTL::Size::Make(16, 16, 1);
            MTL::Size gridSize = MTL::Size::Make(
                threadGroupCountX, threadGroupCountY, threadGroupCountZ
            );

            computeEncoder->dispatchThreadgroups(gridSize, threadgroupSize);
        }

        void transitionBarrier(
            RHITexture*,
            RHIResourceState,
            RHIResourceState after
        ) RHI_OVERRIDE{
            // basicially no-op for Metal.

            if(renderEncoder && after == RHIResourceState::ShaderResource){
                renderEncoder->memoryBarrier(
                    MTL::BarrierScopeTextures,
                    MTL::RenderStageFragment,
                    MTL::RenderStageVertex
                );
            }
        }

        void transitionBarrier(
            RHIBuffer*,
            RHIResourceState,
            RHIResourceState after
        ) RHI_OVERRIDE{
            // basicially no-op for Metal.

            if(renderEncoder && after == RHIResourceState::ShaderResource){
                renderEncoder->memoryBarrier(
                    MTL::BarrierScopeTextures,
                    MTL::RenderStageFragment,
                    MTL::RenderStageVertex
                );
            }
        }

        void uavBarrier(RHITexture*) RHI_OVERRIDE{
            if(computeEncoder) {
                computeEncoder->memoryBarrier(MTL::BarrierScopeTextures);
            }
        }

        void uavBarrier(RHIBuffer*) RHI_OVERRIDE{
            if(computeEncoder) {
                computeEncoder->memoryBarrier(MTL::BarrierScopeBuffers);
            }
        }

        void signalFence(RHIFence* fence, uint64_t value) RHI_OVERRIDE{
            if(!fence || !commandBuffer) return;

            auto metalFence = static_cast<MetalFence*>(fence);
            commandBuffer->encodeSignalEvent(
                metalFence->getSharedEvent(), value
            );
        }

        void waitFence(RHIFence* fence, uint64_t value) RHI_OVERRIDE{
            if(!fence || !commandBuffer) return;

            auto metalFence = static_cast<MetalFence*>(fence);
            commandBuffer->encodeWait(
                metalFence->getSharedEvent(), value
            );
        }

        void copyBuffer(
            RHIBuffer* src,
            RHIBuffer* dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) RHI_OVERRIDE{
            if(!src || !dst) return;

            ensureBlitEncoder();

            auto srcBuf = static_cast<MetalBuffer*>(src)->get();
            auto dstBuf = static_cast<MetalBuffer*>(dst)->get();

            blitEncoder->copyFromBuffer(
                srcBuf, srcOffset,
                dstBuf, dstOffset,
                size
            );
        }

        void copyTexture(
            RHITexture* src,
            RHITexture* dst
        ) RHI_OVERRIDE{
            if(!src || !dst) return;

            ensureBlitEncoder();

            auto srcTex = static_cast<MetalTexture*>(src)->get();
            auto dstTex = static_cast<MetalTexture*>(dst)->get();

            blitEncoder->copyFromTexture(srcTex, dstTex);
        }

        void copyBufferToTexture(
            RHIBuffer* src,
            RHITexture* dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) RHI_OVERRIDE{
            if(!src || !dst) return;

            ensureBlitEncoder();

            auto srcBuf = static_cast<MetalBuffer*>(src)->get();
            auto dstTex = static_cast<MetalTexture*>(dst)->get();

            auto width = dstTex->width();
            auto height = dstTex->height();
            auto bytesPerRow = width * 4;  // TODO: format에 따라 계산
            auto bytesPerImage = bytesPerRow * height;

            blitEncoder->copyFromBuffer(
                srcBuf, 0,
                bytesPerRow, bytesPerImage,
                MTL::Size::Make(width, height, 1),
                dstTex, arraySlice, mipLevel,
                MTL::Origin::Make(0, 0, 0)
            );
        }

        void beginEvent(const char* name) RHI_OVERRIDE{
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

        void endEvent() RHI_OVERRIDE{
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

        void setMarker(const char* name) RHI_OVERRIDE{
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

        MTL::CommandBuffer* getCommandBuffer() const{
            return commandBuffer;
        }

    private:
        void endCurrentEncoder(){
            if(renderEncoder){
                renderEncoder->endEncoding();
                renderEncoder = nullptr;
            }
            if(computeEncoder){
                computeEncoder->endEncoding();
                computeEncoder = nullptr;
            }
            if(blitEncoder){
                blitEncoder->endEncoding();
                blitEncoder = nullptr;
            }
        }

        void ensureBlitEncoder(){
            if(blitEncoder) return;
            endCurrentEncoder();
            blitEncoder = commandBuffer->blitCommandEncoder();
        }
    };
}
