#include <utility>
#include <Metal/MTLBlitCommandEncoder.hpp>
#include <Metal/MTLComputeCommandEncoder.hpp>
#include <Metal/MTLCommandQueue.hpp>
#include "Assert.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalPipelineState.hpp"
#include "MetalUtil.hpp"
#include "MetalTexture.hpp"

namespace Crowy
{
    namespace{
        auto convert(RHILoadAction action){
            using enum RHILoadAction;

            switch(action){
            case Load:     return MTL::LoadActionLoad;
            case Clear:    return MTL::LoadActionClear;
            case DontCare: return MTL::LoadActionDontCare;
            default:
                std::unreachable();
            }
        }

        auto convert(RHIStoreAction action){
            using enum RHIStoreAction;

            switch(action){
            case Store:    return MTL::StoreActionStore;
            case DontCare: return MTL::StoreActionDontCare;
            default:
                std::unreachable();
            }
        }
    }

    MetalCommandList::MetalCommandList(MTL::CommandQueue* queue)
        : commandQueue(queue){}

    MetalCommandList::~MetalCommandList(){
        if(renderEncoder != nullptr){
            renderEncoder->endEncoding();
            renderEncoder = nullptr;
        }
        else if(computeEncoder != nullptr){
            computeEncoder->endEncoding();
            computeEncoder = nullptr;
        }
        else if(blitEncoder != nullptr){
            blitEncoder->endEncoding();
            blitEncoder = nullptr;
        }
    }

    void MetalCommandList::Begin(){
        CROWY_ASSERT(!isRecording,
            "Did you call RHICommandList::Close()?"
        );

        commandBuffer = NS::RetainPtr(commandQueue->commandBuffer());
        commandBuffer->setLabel(toNSString("Crowy Command Buffer"));

        isRecording = true;
    }

    void MetalCommandList::Close(){
        CROWY_ASSERT(isRecording,
            "Did you call RHICommandList::Begin()?"
        );

        CROWY_ASSERT(renderEncoder == nullptr,
            "Did you call RHICommandList::EndRenderPass()?"
        );
        CROWY_ASSERT(computeEncoder == nullptr,
            "Did you call RHICommandList::EndCompute()?"
        );
        CROWY_ASSERT(blitEncoder == nullptr,
            "Did you call RHICommandList::EndBlit()?"
        );

        isRecording = false;
    }

    void MetalCommandList::BeginRenderPass(const RHIRenderPassDesc& desc){
        CROWY_ASSERT(renderEncoder == nullptr,
            "Did you call RHICommandList::EndRenderPass()?"
        );
        CROWY_ASSERT(computeEncoder == nullptr && blitEncoder == nullptr);
        CROWY_ASSERT(desc.colorAttachments.size() > 0);

        auto passDesc = MTL::RenderPassDescriptor::alloc()->init();

        // Color Attachment
        for(usize i=0; i<desc.colorAttachments.size(); ++i){
            auto& attachment = desc.colorAttachments[i];
            auto& mtlAttach = *passDesc->colorAttachments()->object(i);
            mtlAttach.setTexture(static_cast<MetalTexture*>(attachment.texture)->Get());
            mtlAttach.setLoadAction(convert(attachment.loadAction));
            mtlAttach.setStoreAction(convert(attachment.storeAction));
            mtlAttach.setClearColor(MTL::ClearColor::Make(
                attachment.clearColor.x,
                attachment.clearColor.y,
                attachment.clearColor.z,
                attachment.clearColor.w
            ));
        }

        // Depth Attachment
        if(desc.depthAttachment.has_value()){
            auto& attachment = *desc.depthAttachment;
            auto& mtlAttach = *passDesc->depthAttachment();
            mtlAttach.setTexture(static_cast<MetalTexture*>(attachment.texture)->Get());
            mtlAttach.setLoadAction(convert(attachment.loadAction));
            mtlAttach.setStoreAction(convert(attachment.storeAction));
            mtlAttach.setClearDepth(attachment.clearDepthStencil.depth);
        }

        renderEncoder = commandBuffer->renderCommandEncoder(passDesc);
        if(!desc.debugName.empty()){
            renderEncoder->setLabel(toNSString(desc.debugName));
        }

        passDesc->release();
    }

    void MetalCommandList::EndRenderPass(){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        renderEncoder->endEncoding();
        renderEncoder = nullptr;
    }

    void MetalCommandList::SetPipelineState(RHIGraphicsPipelineState& pso){
        auto& metalPSO = static_cast<MetalGraphicsPipelineState&>(pso);
        currentTopology = metalPSO.GetTopology();

        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        metalPSO.Bind(*renderEncoder);
    }

    void MetalCommandList::SetVertexBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 stride,
        u32 offset
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();
        renderEncoder->setVertexBuffer(mtlBuffer, offset, slot);
    }

    void MetalCommandList::SetIndexBuffer(
        RHIBuffer& buffer,
        RHIIndexFormat format,
        u32 offset
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        currentIndexBuffer = static_cast<MetalBuffer&>(buffer).Get();
        currentIndexBufferOffset = offset;
        currentIndexFormat = (format == RHIIndexFormat::UInt16) ?
            MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
    }

    inline constexpr NS::UInteger PushConstantSlot = 0;
    inline constexpr NS::UInteger ConstantBufferSlotBase = 1;

    void MetalCommandList::SetPushGraphicsConstants(
        const void* data,
        u32 size
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        renderEncoder->setVertexBytes(
            data,
            size,
            PushConstantSlot
        );
        renderEncoder->setFragmentBytes(
            data,
            size,
            PushConstantSlot
        );
    }

    void MetalCommandList::SetGraphicsConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 offset
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();

        renderEncoder->setVertexBuffer(
            mtlBuffer,
            offset,
            ConstantBufferSlotBase + slot
        );
        renderEncoder->setFragmentBuffer(
            mtlBuffer,
            offset,
            ConstantBufferSlotBase + slot
        );
    }

    void MetalCommandList::SetViewport(const RHIViewport& viewport){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        MTL::Viewport vp{
            viewport.x, viewport.y,
            viewport.width, viewport.height,
            viewport.minDepth, viewport.maxDepth
        };
        renderEncoder->setViewport(vp);
    }

    void MetalCommandList::SetScissorRect(const RHIScissorRect& scissor){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        MTL::ScissorRect rect{
            static_cast<NS::UInteger>(scissor.left),
            static_cast<NS::UInteger>(scissor.top),
            static_cast<NS::UInteger>(scissor.right - scissor.left),
            static_cast<NS::UInteger>(scissor.bottom - scissor.top)
        };
        renderEncoder->setScissorRect(rect);
    }

    void MetalCommandList::Draw(
        u32 vertexCount,
        u32 instanceCount,
        u32 startVertex,
        u32 startInstance
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        renderEncoder->drawPrimitives(
            currentTopology,
            startVertex,
            vertexCount,
            instanceCount,
            startInstance
        );
    }

    void MetalCommandList::DrawIndexed(
        u32 indexCount,
        u32 instanceCount,
        u32 startIndex,
        int32_t baseVertex,
        u32 startInstance
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(currentIndexBuffer != nullptr);

        auto indexSize = (currentIndexFormat == MTL::IndexTypeUInt16) ? 2 : 4;
        auto indexOffset = currentIndexBufferOffset + startIndex * indexSize;

        renderEncoder->drawIndexedPrimitives(
            currentTopology,
            indexCount,
            currentIndexFormat,
            currentIndexBuffer,
            indexOffset,
            instanceCount,
            baseVertex,
            startInstance
        );
    }

    void MetalCommandList::BeginCompute(){
        CROWY_ASSERT(computeEncoder == nullptr,
            "Did you call RHICommandList::EndCompute()?"
        );
        CROWY_ASSERT(renderEncoder == nullptr && blitEncoder == nullptr);

        threadsPerThreadgroup = {0, 0, 0};
        computeEncoder = commandBuffer->computeCommandEncoder();
    }

    void MetalCommandList::EndCompute(){
        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        computeEncoder->endEncoding();
        computeEncoder = nullptr;
    }

    void MetalCommandList::SetPipelineState(RHIComputePipelineState& pso){
        auto& metalPSO = static_cast<MetalComputePipelineState&>(pso);
        threadsPerThreadgroup = metalPSO.GetThreadsPerThreadgroup();

        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );
        metalPSO.Bind(*computeEncoder);
    }

    void MetalCommandList::SetPushComputeConstants(
        const void* data,
        u32 size
    ){
        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        computeEncoder->setBytes(
            data,
            size,
            PushConstantSlot
        );
    }

    void MetalCommandList::SetComputeConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 offset
    ){
        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();

        computeEncoder->setBuffer(
            mtlBuffer,
            offset,
            ConstantBufferSlotBase + slot
        );
    }

    void MetalCommandList::Dispatch(Size3D gridSize){
        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        auto threadsPerGrid = MTL::Size::Make(
            gridSize.x,
            gridSize.y,
            gridSize.z
        );

        computeEncoder->dispatchThreads(
            threadsPerGrid,
            threadsPerThreadgroup
        );
    }

    void MetalCommandList::BeginBlit(){
        CROWY_ASSERT(blitEncoder == nullptr,
            "Did you call RHICommandList::EndBlit()?"
        );
        CROWY_ASSERT(renderEncoder == nullptr && computeEncoder == nullptr);

        blitEncoder = commandBuffer->blitCommandEncoder();
    }

    void MetalCommandList::EndBlit(){
        CROWY_ASSERT(blitEncoder != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );

        blitEncoder->endEncoding();
        blitEncoder = nullptr;
    }

    void MetalCommandList::Copy(
        RHIBuffer& src,
        RHIBuffer& dst,
        usize srcOffset,
        usize dstOffset,
        usize size
    ){
        CROWY_ASSERT(blitEncoder != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );
        auto srcBuf = static_cast<MetalBuffer&>(src).Get();
        auto dstBuf = static_cast<MetalBuffer&>(dst).Get();

        blitEncoder->copyFromBuffer(
            srcBuf, srcOffset,
            dstBuf, dstOffset,
            size
        );
    }

    void MetalCommandList::Copy(
        RHITexture& src,
        RHITexture& dst
    ){
        CROWY_ASSERT(blitEncoder != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );
        auto srcTex = static_cast<MetalTexture&>(src).Get();
        auto dstTex = static_cast<MetalTexture&>(dst).Get();

        blitEncoder->copyFromTexture(srcTex, dstTex);
    }

    void MetalCommandList::Copy(
        RHIBuffer& src,
        u64 srcOffset,
        u32 srcRowPitch,
        RHITexture& dst,
        const RHITextureRegion& region,
        u32 mipLevel,
        u32 arraySlice
    ){
        CROWY_ASSERT(blitEncoder != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );
        auto srcBuf = static_cast<MetalBuffer&>(src).Get();
        auto& metalDst = static_cast<MetalTexture&>(dst);
        auto dstTex = metalDst.Get();

        CROWY_ASSERT(
            region.x + region.width <= metalDst.GetWidth(mipLevel) &&
            region.y + region.height <= metalDst.GetHeight(mipLevel),
            "copy region reaches past the mip"
        );

        // Single 2D slice, so depth is 1 and the origin's z is 0.
        const auto sourceSize = MTL::Size::Make(
            region.width,
            region.height,
            1
        );
        const auto destinationOrigin = MTL::Origin::Make(
            region.x,
            region.y,
            0
        );

        blitEncoder->copyFromBuffer(
            srcBuf,
            srcOffset,
            srcRowPitch,
            srcRowPitch * region.height,
            sourceSize,
            dstTex,
            arraySlice,
            mipLevel,
            destinationOrigin
        );
    }

    void MetalCommandList::TransitionBarrier(
        std::span<const RHITextureBarrier> textureBarriers,
        std::span<const RHIBufferBarrier> bufferBarriers
    ){
        // Metal has implicit synchronization between render passes,
        // so we only need to track state for API consistency.
        // memoryBarrier is only needed for same-pass synchronization.
        for(auto& barrier: textureBarriers){
            auto& resource = barrier.texture;
            const auto after = barrier.point;

            resource.TransitionState(after, barrier.range);
        }
        for(auto& barrier: bufferBarriers){
            auto& resource = barrier.buffer;

            resource.TransitionState(barrier.syncAfter);
            resource.TransitionState(barrier.accessAfter);
        }
    }

    void MetalCommandList::WaitUntilCompleted(){
        commandBuffer->waitUntilCompleted();
    }

    void MetalCommandList::BeginEvent(CStr name){
        auto str = toNSString(name);
        if(renderEncoder != nullptr){
            renderEncoder->pushDebugGroup(str);
        }
        else if(computeEncoder != nullptr){
            computeEncoder->pushDebugGroup(str);
        }
        else if(blitEncoder != nullptr){
            blitEncoder->pushDebugGroup(str);
        }
    }

    void MetalCommandList::EndEvent(){
        if(renderEncoder != nullptr){
            renderEncoder->popDebugGroup();
        }
        else if(computeEncoder != nullptr){
            computeEncoder->popDebugGroup();
        }
        else if(blitEncoder != nullptr){
            blitEncoder->popDebugGroup();
        }
    }

    void MetalCommandList::SetMarker(CStr name){
        auto str = toNSString(name);
        if(renderEncoder != nullptr){
            renderEncoder->insertDebugSignpost(str);
        }
        else if(computeEncoder != nullptr){
            computeEncoder->insertDebugSignpost(str);
        }
        else if(blitEncoder != nullptr){
            blitEncoder->insertDebugSignpost(str);
        }
    }
}
