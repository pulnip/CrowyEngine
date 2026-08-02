#include <utility>
#include <Metal/MTLBlitCommandEncoder.hpp>
#include <Metal/MTLComputeCommandEncoder.hpp>
#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLFence.hpp>
#include "Assert.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalPipelineState.hpp"
#include "MetalUtil.hpp"
#include "MetalTexture.hpp"

#include <stdexcept>

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

    MetalCommandList::MetalCommandList(
        MTL::CommandQueue* queue,
        MTL::Fence* barrier
    )
        : commandQueue(queue)
        , barrier(barrier){}

    MetalCommandList::~MetalCommandList(){
        // end whatever pass is still open
        std::visit([](auto& state){
            if constexpr(requires{ state.encoder; }){
                if(state.encoder != nullptr){
                    state.encoder->endEncoding();
                }
            }
        }, passState);
        passState = std::monostate{};
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

        CROWY_ASSERT(std::holds_alternative<std::monostate>(passState),
            "Did you call RHICommandList::EndRenderPass()/EndCompute()/EndBlit()?"
        );

        isRecording = false;
    }

    void MetalCommandList::BeginRenderPass(const RHIRenderPassDesc& desc){
        CROWY_ASSERT(std::holds_alternative<std::monostate>(passState),
            "Did you call RHICommandList::EndRenderPass()?"
        );
        CROWY_ASSERT(desc.colorAttachments.size() > 0);

        auto passDesc = MTL::RenderPassDescriptor::alloc()->init();

        // Color Attachment
        for(usize i=0; i<desc.colorAttachments.size(); ++i){
            auto& attachment = desc.colorAttachments[i];
            auto& mtlAttach = *passDesc->colorAttachments()->object(i);
            mtlAttach.setTexture(static_cast<MetalTexture*>(attachment.texture)->Get());
            mtlAttach.setLevel(attachment.mipLevel);
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
            mtlAttach.setLevel(attachment.mipLevel);
            mtlAttach.setLoadAction(convert(attachment.loadAction));
            mtlAttach.setStoreAction(convert(attachment.storeAction));
            mtlAttach.setClearDepth(attachment.clearDepthStencil.depth);
        }

        auto encoder = commandBuffer->renderCommandEncoder(passDesc);
        if(!desc.debugName.empty()){
            encoder->setLabel(toNSString(desc.debugName));
        }

        passDesc->release();

        if(barrierPending){
            encoder->waitForFence(
                barrier,
                MTL::RenderStageVertex
            );
            barrierPending = false;
        }

        passState = RenderPassState{
            .encoder = encoder
        };
    }

    void MetalCommandList::EndRenderPass(){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        state->encoder->updateFence(
            barrier,
            MTL::RenderStageFragment
        );
        state->encoder->endEncoding();
        passState = std::monostate{};
    }

    void MetalCommandList::SetPipelineState(RHIGraphicsPipelineState& pso){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        auto& metalPSO = static_cast<MetalGraphicsPipelineState&>(pso);
        metalPSO.Bind(*state->encoder);

        state->topology = metalPSO.GetTopology();
        state->pso = &metalPSO;
    }

    void MetalCommandList::SetVertexBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 stride,
        u32 offset
    ){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(slot < MaxVertexBufferSlots);

        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();
        state->encoder->setVertexBuffer(
            mtlBuffer,
            offset,
            toVertexBufferIndex(slot)
        );
    }

    void MetalCommandList::SetIndexBuffer(
        RHIBuffer& buffer,
        RHIIndexFormat format,
        u32 offset
    ){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        state->indexBuffer = static_cast<MetalBuffer&>(buffer).Get();
        state->indexBufferOffset = offset;
        state->indexFormat = (format == RHIIndexFormat::UInt16) ?
            MTL::IndexTypeUInt16 : MTL::IndexTypeUInt32;
    }

    inline constexpr NS::UInteger PushConstantSlot = 0;
    inline constexpr NS::UInteger ConstantBufferSlotBase = 1;

    void MetalCommandList::SetPushGraphicsConstants(
        const void* data,
        u32 size
    ){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(state->pso != nullptr,
            "Did you call RHICommandList::SetPipelineState()?"
        );

        if(state->pso->UsesVertexBuffer(PushConstantSlot)){
            state->encoder->setVertexBytes(
                data,
                size,
                PushConstantSlot
            );
        }
        if(state->pso->UsesFragmentBuffer(PushConstantSlot)){
            state->encoder->setFragmentBytes(
                data,
                size,
                PushConstantSlot
            );
        }
    }

    void MetalCommandList::SetGraphicsConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 offset
    ){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(state->pso != nullptr,
            "Did you call RHICommandList::SetPipelineState()?"
        );
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();
        const auto index = ConstantBufferSlotBase + slot;

        if(state->pso->UsesVertexBuffer(index)){
            state->encoder->setVertexBuffer(
                mtlBuffer,
                offset,
                index
            );
        }
        if(state->pso->UsesFragmentBuffer(index)){
            state->encoder->setFragmentBuffer(
                mtlBuffer,
                offset,
                index
            );
        }
    }

    void MetalCommandList::SetViewport(const RHIViewport& viewport){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        MTL::Viewport vp{
            viewport.x, viewport.y,
            viewport.width, viewport.height,
            viewport.minDepth, viewport.maxDepth
        };
        state->encoder->setViewport(vp);
    }

    void MetalCommandList::SetScissorRect(const RHIScissorRect& scissor){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        MTL::ScissorRect rect{
            static_cast<NS::UInteger>(scissor.left),
            static_cast<NS::UInteger>(scissor.top),
            static_cast<NS::UInteger>(scissor.right - scissor.left),
            static_cast<NS::UInteger>(scissor.bottom - scissor.top)
        };
        state->encoder->setScissorRect(rect);
    }

    void MetalCommandList::Draw(
        u32 vertexCount,
        u32 instanceCount,
        u32 startVertex,
        u32 startInstance
    ){
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        state->encoder->drawPrimitives(
            state->topology,
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
        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(state->indexBuffer != nullptr);

        auto indexSize = (state->indexFormat == MTL::IndexTypeUInt16) ? 2 : 4;
        auto indexOffset = state->indexBufferOffset + startIndex * indexSize;

        state->encoder->drawIndexedPrimitives(
            state->topology,
            indexCount,
            state->indexFormat,
            state->indexBuffer,
            indexOffset,
            instanceCount,
            baseVertex,
            startInstance
        );
    }

    void MetalCommandList::ExecuteIndirect(const DrawBatch&){
        throw std::runtime_error("Unimplemented");
    }

    void MetalCommandList::BeginCompute(){
        CROWY_ASSERT(std::holds_alternative<std::monostate>(passState),
            "Did you call RHICommandList::EndCompute()?"
        );

        auto encoder = commandBuffer->computeCommandEncoder();

        if(barrierPending){
            encoder->waitForFence(barrier);
            barrierPending = false;
        }

        passState = ComputePassState{
            .encoder = encoder
        };
    }

    void MetalCommandList::EndCompute(){
        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        state->encoder->updateFence(barrier);
        state->encoder->endEncoding();
        passState = std::monostate{};
    }

    void MetalCommandList::SetPipelineState(RHIComputePipelineState& pso){
        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        auto& metalPSO = static_cast<MetalComputePipelineState&>(pso);
        metalPSO.Bind(*state->encoder);

        state->threadsPerThreadgroup = metalPSO.GetThreadsPerThreadgroup();
    }

    void MetalCommandList::SetPushComputeConstants(
        const void* data,
        u32 size
    ){
        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        state->encoder->setBytes(
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
        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();

        state->encoder->setBuffer(
            mtlBuffer,
            offset,
            ConstantBufferSlotBase + slot
        );
    }

    void MetalCommandList::Dispatch(Size3D gridSize){
        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginCompute()?"
        );

        auto threadsPerGrid = MTL::Size::Make(
            gridSize.x,
            gridSize.y,
            gridSize.z
        );

        state->encoder->dispatchThreads(
            threadsPerGrid,
            state->threadsPerThreadgroup
        );
    }

    void MetalCommandList::BeginBlit(){
        CROWY_ASSERT(std::holds_alternative<std::monostate>(passState),
            "Did you call RHICommandList::EndBlit()?"
        );

        auto encoder = commandBuffer->blitCommandEncoder();

        if(barrierPending){
            encoder->waitForFence(barrier);
            barrierPending = false;
        }

        passState = BlitPassState{
            .encoder = encoder
        };
    }

    void MetalCommandList::EndBlit(){
        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );

        state->encoder->updateFence(barrier);
        state->encoder->endEncoding();
        passState = std::monostate{};
    }

    void MetalCommandList::Copy(
        RHIBuffer& src,
        RHIBuffer& dst,
        usize srcOffset,
        usize dstOffset,
        usize size
    ){
        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );
        auto srcBuf = static_cast<MetalBuffer&>(src).Get();
        auto dstBuf = static_cast<MetalBuffer&>(dst).Get();

        state->encoder->copyFromBuffer(
            srcBuf, srcOffset,
            dstBuf, dstOffset,
            size
        );
    }

    void MetalCommandList::Copy(
        RHITexture& src,
        RHITexture& dst
    ){
        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlit()?"
        );
        auto srcTex = static_cast<MetalTexture&>(src).Get();
        auto dstTex = static_cast<MetalTexture&>(dst).Get();

        state->encoder->copyFromTexture(srcTex, dstTex);
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
        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
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

        state->encoder->copyFromBuffer(
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
        if(auto state = std::get_if<ComputePassState>(&passState)){
            MTL::BarrierScope scope = 0;
            if(!bufferBarriers.empty()){
                scope |= MTL::BarrierScopeBuffers;
            }
            if(!textureBarriers.empty()){
                scope |= MTL::BarrierScopeTextures;
            }
            state->encoder->memoryBarrier(scope);
        }
        else if(std::holds_alternative<std::monostate>(passState)){
            // commands never overlap on the same untracked resource within one pass
            // so state tracking alone is enough
            barrierPending = true;
        }

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
        std::visit([str](auto& state){
            if constexpr(requires{ state.encoder; }){
                state.encoder->pushDebugGroup(str);
            }
        }, passState);
    }

    void MetalCommandList::EndEvent(){
        std::visit([](auto& state){
            if constexpr(requires{ state.encoder; }){
                state.encoder->popDebugGroup();
            }
        }, passState);
    }

    void MetalCommandList::SetMarker(CStr name){
        auto str = toNSString(name);
        std::visit([str](auto& state){
            if constexpr(requires{ state.encoder; }){
                state.encoder->insertDebugSignpost(str);
            }
        }, passState);
    }
}
