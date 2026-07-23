#include <stdexcept>
#include <utility>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include "Assert.hpp"
#include "MetalBuffer.hpp"
#include "MetalPipelineState.hpp"
#include "MetalSampler.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"
#include "RHIDefinitions.hpp"

#include "MetalCommandList.hpp"

namespace{
    auto convert(Crowy::RHILoadAction action){
        using namespace Crowy;
        using enum RHILoadAction;
        using namespace MTL;

        switch(action){
        case Load:     return LoadActionLoad;
        case Clear:    return LoadActionClear;
        case DontCare: return LoadActionDontCare;
        default:
            std::unreachable();
        }
    }

    auto convert(Crowy::RHIStoreAction action){
        using namespace Crowy;
        using enum RHIStoreAction;
        using namespace MTL;

        switch(action){
        case Store:    return StoreActionStore;
        case DontCare: return StoreActionDontCare;
        default:
            std::unreachable();
        }
    }
}

namespace Crowy
{
    MetalCommandList::MetalCommandList(MTL::CommandQueue* queue)
        : commandQueue(queue){}

    MetalCommandList::~MetalCommandList(){
        Reset();
    }

    void MetalCommandList::Begin(){
        CROWY_ASSERT(!isRecording,
            "Did you call RHICommandList::close()?"
        );

        commandBuffer = commandQueue->commandBuffer();
        commandBuffer->setLabel(
            NS::String::string("Crowy Command Buffer", NS::UTF8StringEncoding)
        );
        isRecording = true;
    }

    void MetalCommandList::Flush(){
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

    void MetalCommandList::Close(){
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

    void MetalCommandList::Reset(){
        if(isRecording){
            Flush();

            isRecording = false;
        }
        else{
            CROWY_ASSERT( renderEncoder == nullptr);
            CROWY_ASSERT(computeEncoder == nullptr);
            CROWY_ASSERT(   blitEncoder == nullptr);
        }
    }

    void MetalCommandList::BeginRenderPass(const RHIRenderPassDesc& desc){
        CROWY_ASSERT(renderEncoder == nullptr,
            "Did you call RHICommandList::endRenderPass()?"
        );
        CROWY_ASSERT(computeEncoder == nullptr);
        if(blitEncoder != nullptr){
            blitEncoder->endEncoding();
            blitEncoder = nullptr;
        }
        CROWY_ASSERT(desc.colorAttachments.size() > 0);

        auto passDesc = MTL::RenderPassDescriptor::alloc()->init();

        // Color Attachment
        for(usize i=0; i<desc.colorAttachments.size(); ++i){
            auto& attachment = desc.colorAttachments[i];
            auto& mtlAttach = *passDesc->colorAttachments()->object(i);
            mtlAttach.setTexture(static_cast<MetalTexture*>(attachment.texture)->Get());
            mtlAttach.setLoadAction(::convert(attachment.loadAction));
            mtlAttach.setStoreAction(::convert(attachment.storeAction));
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
            mtlAttach.setLoadAction(::convert(attachment.loadAction));
            mtlAttach.setStoreAction(::convert(attachment.storeAction));
            mtlAttach.setClearDepth(attachment.clearDepthStencil.depth);
        }

        renderEncoder = commandBuffer->renderCommandEncoder(passDesc);
        if(!desc.debugName.empty()){
            renderEncoder->setLabel(
                NS::String::string(desc.debugName.c_str(), NS::UTF8StringEncoding)
            );
        }

        passDesc->release();
    }

    void MetalCommandList::EndRenderPass(){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::beginRenderPass()?"
        );

        renderEncoder->endEncoding();
        renderEncoder = nullptr;
    }

    void MetalCommandList::SetPipelineState(RHIGraphicsPipelineState& pso){
        auto& metalPSO = static_cast<MetalGraphicsPipelineState&>(pso);
        currentTopology = metalPSO.GetTopology();

        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::beginRenderPass()?"
        );
        metalPSO.Bind(*renderEncoder);
    }

    void MetalCommandList::SetPipelineState(RHIComputePipelineState& pso){
        auto& metalPSO = static_cast<MetalComputePipelineState&>(pso);
        threadsPerThreadgroup = metalPSO.GetThreadsPerThreadgroup();

        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::beginCompute()?"
        );
        metalPSO.Bind(*computeEncoder);
    }

    void MetalCommandList::SetVertexBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 stride,
        u32 offset
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::beginRenderPass()?"
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
            "Did you call RHICommandList::beginRenderPass()?"
        );

        currentIndexBuffer = static_cast<MetalBuffer&>(buffer).Get();
        currentIndexBufferOffset = offset;
        currentIndexFormat = (format == RHIIndexFormat::UInt16) ?
            MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
    }

    void MetalCommandList::SetConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        RHIShaderStage stage,
        u32 offset
    ){
        using enum RHIShaderStage;
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();

        switch(stage){
        case VertexShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setVertexBuffer(mtlBuffer, offset, slot);
            break;
        case FragmentShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setFragmentBuffer(mtlBuffer, offset, slot);
            break;
        case ComputeShader:
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->setBuffer(mtlBuffer, offset, slot);
            break;
        default:
            std::unreachable();
        }
    }

    void MetalCommandList::SetTexture(
        RHITexture& texture,
        u32 slot,
        RHIBindingAccess,
        RHIShaderStage stage
    ){
        using enum RHIShaderStage;
        auto mtlTexture = static_cast<MetalTexture&>(texture).Get();

        switch(stage){
        case VertexShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setVertexTexture(mtlTexture, slot);
            break;
        case FragmentShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setFragmentTexture(mtlTexture, slot);
            break;
        case ComputeShader:
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->setTexture(mtlTexture, slot);
            break;
        default:
            std::unreachable();
        }
    }

    void MetalCommandList::SetBuffer(
        RHIBuffer& buffer,
        u32 slot,
        RHIBindingAccess,
        RHIShaderStage stage
    ){
        using enum RHIShaderStage;
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();

        switch(stage){
        case VertexShader:
            [[fallthrough]];
        case FragmentShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            throw std::runtime_error("Unimplemented");
        case ComputeShader:
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->setBuffer(mtlBuffer, 0, slot);
            break;
        default:
            std::unreachable();
        }
    }

    void MetalCommandList::SetBytes(
        const void* bytes,
        usize size,
        u32 slot,
        RHIShaderStage stage
    ){
        using enum RHIShaderStage;

        switch(stage){
        case VertexShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setVertexBytes(bytes, size, slot);
            break;
        case FragmentShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setFragmentBytes(bytes, size, slot);
            break;
        case ComputeShader:
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->setBytes(bytes, size, slot);
            break;
        default:
            std::unreachable();
        }
    }

    void MetalCommandList::SetSampler(
        RHISampler& sampler,
        u32 slot,
        RHIShaderStage stage
    ){
        using enum RHIShaderStage;
        auto mtlSampler = static_cast<MetalSampler&>(sampler).Get();

        switch(stage){
        case VertexShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setVertexSamplerState(mtlSampler, slot);
            break;
        case FragmentShader:
            CROWY_ASSERT(renderEncoder != nullptr,
                "Did you call RHICommandList::beginRenderPass()?"
            );
            renderEncoder->setFragmentSamplerState(mtlSampler, slot);
            break;
        case ComputeShader:
            CROWY_ASSERT(computeEncoder != nullptr,
                "Did you call RHICommandList::beginCompute()?"
            );
            computeEncoder->setSamplerState(mtlSampler, slot);
            break;
        default:
            std::unreachable();
        }
    }

    void MetalCommandList::SetViewport(const RHIViewport& viewport){
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

    void MetalCommandList::SetScissorRect(const RHIScissorRect& scissor){
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

    void MetalCommandList::Draw(
        u32 vertexCount,
        u32 instanceCount,
        u32 startVertex,
        u32 startInstance
    ){
        CROWY_ASSERT(renderEncoder != nullptr,
            "Did you call RHICommandList::beginRenderPass()?"
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
            "Did you call RHICommandList::beginRenderPass()?"
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
            "Did you call RHICommandList::endCompute()?"
        );
        CROWY_ASSERT(renderEncoder == nullptr);
        if(blitEncoder != nullptr){
            blitEncoder->endEncoding();
            blitEncoder = nullptr;
        }

        threadsPerThreadgroup = {0, 0, 0};
        computeEncoder = commandBuffer->computeCommandEncoder();
    }

    void MetalCommandList::EndCompute(){
        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::beginCompute()?"
        );

        computeEncoder->endEncoding();
        computeEncoder = nullptr;
    }

    void MetalCommandList::Dispatch(Size3D gridSize){
        CROWY_ASSERT(computeEncoder != nullptr,
            "Did you call RHICommandList::beginCompute()?"
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

    void MetalCommandList::TransitionBarrier(
        RHITexture& texture,
        RHIResourceState after
    ){
        // Metal has implicit synchronization between render passes,
        // so we only need to track state for API consistency.
        // memoryBarrier is only needed for same-pass synchronization.
        texture.SetState(after);
    }

    void MetalCommandList::TransitionBarrier(
        RHIBuffer& buffer,
        RHIResourceState after
    ){
        // no-op for Metal
        buffer.SetState(after);
    }

    void MetalCommandList::Copy(
        RHIBuffer& src,
        RHIBuffer& dst,
        usize srcOffset,
        usize dstOffset,
        usize size
    ){
        ensureBlitEncoder();

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
        ensureBlitEncoder();

        auto srcTex = static_cast<MetalTexture&>(src).Get();
        auto dstTex = static_cast<MetalTexture&>(dst).Get();

        blitEncoder->copyFromTexture(srcTex, dstTex);
    }

    void MetalCommandList::Copy(
        RHITexture& src,
        RHISwapchain& swapchain
    ){
        ensureBlitEncoder();

        auto& mtlSwapchain = static_cast<MetalSwapchain&>(swapchain);

        auto& srcTex = static_cast<MetalTexture&>(src);
        auto& dstTex = static_cast<MetalTexture&>(mtlSwapchain.GetCurrentTexture());

        blitEncoder->copyFromTexture(srcTex.Get(), dstTex.Get());
    }

    void MetalCommandList::Copy(
        RHIBuffer& src,
        RHITexture& dst,
        u32 mipLevel,
        u32 arraySlice
    ){
        ensureBlitEncoder();

        auto srcBuf = static_cast<MetalBuffer&>(src).Get();
        auto dstTex = static_cast<MetalTexture&>(dst).Get();

        auto width = dstTex->width();
        auto height = dstTex->height();
        auto bytesPerRow = width * getBytesPerPixel(dst.GetFormat());
        auto bytesPerImage = bytesPerRow * height;

        blitEncoder->copyFromBuffer(
            srcBuf, 0,
            bytesPerRow, bytesPerImage,
            MTL::Size::Make(width, height, 1),
            dstTex, arraySlice, mipLevel,
            MTL::Origin::Make(0, 0, 0)
        );
    }

    void MetalCommandList::WaitUntilCompleted(){
        commandBuffer->waitUntilCompleted();
    }

    void MetalCommandList::BeginEvent(CStr name){
        auto str = NS::String::string(name, NS::UTF8StringEncoding);
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
        auto str = NS::String::string(name, NS::UTF8StringEncoding);
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

    void MetalCommandList::ensureBlitEncoder(){
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
}
