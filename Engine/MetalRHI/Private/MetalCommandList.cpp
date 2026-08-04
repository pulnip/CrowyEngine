#include <cstring>
#include <utility>
#include <Metal/MTLBlitCommandEncoder.hpp>
#include <Metal/MTLComputeCommandEncoder.hpp>
#include <Metal/MTLCommandQueue.hpp>
#include <Metal/MTLDevice.hpp>
#include "Assert.hpp"
#include "EnumUtil.hpp"
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

        // sync → fence stage. the two directions round differently:
        // a producer signals after its latest stage,
        // a consumer waits before its earliest.
        MTL::RenderStages lastStage(RHIBarrierSync sync){
            constexpr auto fragish = combine(
                RHIBarrierSync::All,
                RHIBarrierSync::PixelShading,
                RHIBarrierSync::DepthStencil,
                RHIBarrierSync::RenderTarget
            );
            return hasFlag(sync, fragish) ?
                MTL::RenderStageFragment :
                MTL::RenderStageVertex;
        }
        MTL::RenderStages firstStage(RHIBarrierSync sync){
            constexpr auto vertish = combine(
                RHIBarrierSync::All,
                RHIBarrierSync::Draw,
                RHIBarrierSync::IndexInput,
                RHIBarrierSync::VertexShading,
                RHIBarrierSync::ExecuteIndirect
            );
            return hasFlag(sync, vertish) ?
                MTL::RenderStageVertex :
                MTL::RenderStageFragment;
        }
    }

    MetalCommandList::MetalCommandList(
        MTL::CommandQueue* queue,
        MTL::Event* submissionEvent,
        const u64& submissionSerial,
        const u64& handoffSerial
    )
        : commandQueue(queue)
        , submissionEvent(submissionEvent)
        , submissionSerial(submissionSerial)
        , handoffSerial(handoffSerial){}

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
        Super::Begin();

        CROWY_ASSERT(!isRecording,
            "Did you call RHICommandList::Close()?"
        );

        commandBuffer = NS::RetainPtr(commandQueue->commandBuffer());
        commandBuffer->setLabel(toNSString("Crowy Command Buffer"));

        submissionWaitEncoded = false;
        fenceCursor = 0;
        pendingTextureReleases.clear();
        pendingBufferReleases.clear();

        // hangover gate: acquire-less consumption of a hand-off release rides queue order,
        // so a recording that may share the GPU with the hand-off wave must be ordered behind it.
        // anything older is already CPU-fenced - FramePacer::BeginFrame
        // waits the (frameIndex - RHI_FRAMES_IN_FLIGHT + 1) fence before recording,
        // and out-of-band frameIndex bumps only narrow the true window.
        if(handoffSerial != 0 &&
            submissionSerial < handoffSerial + RHI_FRAMES_IN_FLIGHT - 1
        ){
            encodeSubmissionWait();
        }

        isRecording = true;
    }

    void MetalCommandList::Close(){
        Super::Close();

        CROWY_ASSERT(isRecording,
            "Did you call RHICommandList::Begin()?"
        );

        // un-acquired pending releases are the cross-submission hand-off:
        // their fences are already updated behind the producers, and later
        // submissions order against them through the submission event,
        // so nothing records here (unlike DX12's Close-flush).
        isRecording = false;
    }

    void MetalCommandList::encodeSubmissionWait(){
        if(submissionWaitEncoded){
            return;
        }

        // gates every encoder recorded after this point behind the
        // completion of all previously submitted waves
        commandBuffer->encodeWait(submissionEvent, submissionSerial);
        submissionWaitEncoded = true;
    }

    bool MetalCommandList::HasUnconsumedReleases() const noexcept{
        for(const auto& [_, entries]: pendingTextureReleases){
            for(const auto& entry: entries){
                if(!entry.consumed){
                    return true;
                }
            }
        }
        for(const auto& [_, entry]: pendingBufferReleases){
            if(!entry.consumed){
                return true;
            }
        }
        return false;
    }

    MTL::Fence* MetalCommandList::acquireFence(){
        if(fenceCursor == fencePool.size()){
            auto fence = NS::TransferPtr(
                commandQueue->device()->newFence()
            );
        #if defined(_DEBUG) || !defined(NDEBUG)
            fence->setLabel(toNSString("Crowy Edge Fence"));
        #endif
            fencePool.push_back(std::move(fence));
        }
        return fencePool[fenceCursor++].get();
    }

    MTL::Fence* MetalCommandList::findPending(
        const RHITextureBarrier& acquire
    ){
        if(const auto it = pendingTextureReleases.find(acquire.texture);
            it != pendingTextureReleases.end()
        ){
            for(auto& entry: it->second){
                // full-edge equality doubles as the §5 cross validation:
                // both halves carried the same value or they do not pair
                if(entry.barrier == acquire){
                    entry.consumed = true;
                    return entry.fence;
                }
            }
        }
        return nullptr;
    }

    MTL::Fence* MetalCommandList::findPending(
        const RHIBufferBarrier& acquire
    ){
        if(const auto it = pendingBufferReleases.find(acquire.buffer);
            it != pendingBufferReleases.end() &&
            it->second.barrier == acquire
        ){
            it->second.consumed = true;
            return it->second.fence;
        }
        return nullptr;
    }

    void MetalCommandList::gatherWaits(
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        waitScratch.clear();

        const auto gather = [this](const auto& acquire){
            if(const auto fence = findPending(acquire)){
                for(auto& wait: waitScratch){
                    if(wait.fence == fence){
                        wait.syncAfter = combine(
                            wait.syncAfter,
                            acquire.syncAfter
                        );
                        return;
                    }
                }
                waitScratch.push_back({fence, acquire.syncAfter});
                return;
            }

            CROWY_ASSERT(
                acquire.syncBefore == RHIBarrierSync::None ||
                acquire.crossSubmission,
                "acquire with real before-sync has no matching release in "
                "this command list; producers in earlier submissions need "
                "the crossSubmission flag (MakeCrossSubmissionBarrier)"
            );
            if(acquire.crossSubmission){
                // lazy gate: passes before the first cross-submission
                // acquire keep overlapping the previous wave; from this
                // pass on, the queue-global ordering the acquire relies
                // on is restored
                encodeSubmissionWait();
            }
            // plain self-contained acquires wait on nothing:
            // first use has no producer
        };
        for(const auto& acquire: textureAcquires){
            gather(acquire);
        }
        for(const auto& acquire: bufferAcquires){
            gather(acquire);
        }
    }

    void MetalCommandList::parkRelease(
        const RHITextureBarrier& release,
        MTL::Fence* fence
    ){
        auto& pending = pendingTextureReleases[release.texture];
        for(auto& entry: pending){
            if(entry.barrier.range == release.range){
                // re-release of the same range = new producer. the old edge
                // just goes stale - layout is a no-op here, so unlike DX12
                // there is no transition left to complete
                entry = {.barrier = release, .fence = fence};
                return;
            }
        }
        pending.push_back({.barrier = release, .fence = fence});
    }

    void MetalCommandList::parkRelease(
        const RHIBufferBarrier& release,
        MTL::Fence* fence
    ){
        pendingBufferReleases.insert_or_assign(
            release.buffer,
            PendingRelease<RHIBufferBarrier>{
                .barrier = release,
                .fence = fence
            }
        );
    }

    MTL::Fence* MetalCommandList::parkReleases(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases,
        RHIBarrierSync& syncBeforeUnion
    ){
        MTL::Fence* fence = nullptr;
        syncBeforeUnion = RHIBarrierSync::None;

        const auto park = [&, this](const auto& release){
            if(release.syncAfter == RHIBarrierSync::None){
                // self-contained release (e.g. RenderTarget → Present):
                // what follows is guaranteed elsewhere and layout is a
                // Metal no-op, so nothing records at all
                return;
            }
            if(fence == nullptr){
                fence = acquireFence();
            }
            syncBeforeUnion = combine(syncBeforeUnion, release.syncBefore);
            parkRelease(release, fence);
        };
        for(const auto& release: textureReleases){
            park(release);
        }
        for(const auto& release: bufferReleases){
            park(release);
        }
        return fence;
    }

    void MetalCommandList::BeginRenderPass(
        const RHIRenderPassDesc& desc,
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        Super::BeginRenderPass(desc, textureAcquires, bufferAcquires);

        gatherWaits(textureAcquires, bufferAcquires);

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
    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!desc.debugName.empty()){
            encoder->setLabel(toNSString(desc.debugName));
        }
    #endif

        passDesc->release();

        // several distinct producer fences can land here - that is the
        // non-adjacent A-B-C dependency working as intended
        for(const auto& wait: waitScratch){
            encoder->waitForFence(wait.fence, firstStage(wait.syncAfter));
        }

        passState = RenderPassState{
            .encoder = encoder
        };
    }

    void MetalCommandList::EndRenderPass(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        Super::EndRenderPass(textureReleases, bufferReleases);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        RHIBarrierSync syncBeforeUnion;
        if(const auto fence = parkReleases(
            textureReleases,
            bufferReleases,
            syncBeforeUnion
        )){
            state->encoder->updateFence(
                fence,
                lastStage(syncBeforeUnion)
            );
        }

        state->encoder->endEncoding();
        passState = std::monostate{};
    }

    void MetalCommandList::SetPipelineState(RHIGraphicsPipelineState& pso){
        Super::SetPipelineState(pso);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        auto& metalPSO = static_cast<MetalGraphicsPipelineState&>(pso);
        metalPSO.Bind(*state->encoder);

        state->topology = metalPSO.GetTopology();
        state->vsUsedBufferMask = metalPSO.GetVSUsedBufferMask();
        state->fsUsedBufferMask = metalPSO.GetFSUsedBufferMask();

        constexpr u32 CB_ALL_CLEAN = (1u << RHI_NUM_DIRECT_CBS) - 1u;
        state->pushDirty = true;
        state->cbDirtyMask = CB_ALL_CLEAN;
    }

    void MetalCommandList::SetVertexBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 stride,
        u32 offset
    ){
        Super::SetVertexBuffer(buffer, slot, stride, offset);

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
        Super::SetIndexBuffer(buffer, format, offset);

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
        Super::SetPushGraphicsConstants(data, size);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        std::memcpy(state->pushConstants.data(), data, size);
        state->pushConstantSize = size;
        state->pushDirty = true;
    }

    void MetalCommandList::applyPushConstants(RenderPassState& state){
        if(state.pushConstantSize == 0){
            return;
        }

        if((state.vsUsedBufferMask >> PushConstantSlot) & 1u){
            state.encoder->setVertexBytes(
                state.pushConstants.data(),
                state.pushConstantSize,
                PushConstantSlot
            );
        }
        if((state.fsUsedBufferMask >> PushConstantSlot) & 1u){
            state.encoder->setFragmentBytes(
                state.pushConstants.data(),
                state.pushConstantSize,
                PushConstantSlot
            );
        }
    }

    void MetalCommandList::SetGraphicsConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 offset
    ){
        Super::SetGraphicsConstantBuffer(buffer, slot, offset);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        state->constantBuffers[slot] = {
            .buffer = static_cast<MetalBuffer&>(buffer).Get(),
            .offset = offset
        };
        state->cbDirtyMask |= 1u << slot;
    }

    void MetalCommandList::applyConstantBuffer(
        RenderPassState& state,
        u32 slot
    ){
        const auto& binding = state.constantBuffers[slot];
        if(binding.buffer == nullptr){
            return;
        }
        const auto index = ConstantBufferSlotBase + slot;

        if((state.vsUsedBufferMask >> index) & 1u){
            state.encoder->setVertexBuffer(
                binding.buffer,
                binding.offset,
                index
            );
        }
        if((state.fsUsedBufferMask >> index) & 1u){
            state.encoder->setFragmentBuffer(
                binding.buffer,
                binding.offset,
                index
            );
        }
    }

    void MetalCommandList::flush(RenderPassState& state){
        if(state.pushDirty){
            applyPushConstants(state);
            state.pushDirty = false;
        }
        for(u32 slot=0; slot<RHI_NUM_DIRECT_CBS; ++slot){
            if(state.cbDirtyMask & (1u << slot)){
                applyConstantBuffer(state, slot);
            }
        }
        state.cbDirtyMask = 0;
    }

    void MetalCommandList::SetViewport(const RHIViewport& viewport){
        Super::SetViewport(viewport);

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
        Super::SetScissorRect(scissor);

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
        Super::Draw(vertexCount, instanceCount, startVertex, startInstance);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );

        flush(*state);
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
        i32 baseVertex,
        u32 startInstance
    ){
        Super::DrawIndexed(indexCount, instanceCount, startIndex, baseVertex, startInstance);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(state->indexBuffer != nullptr);

        flush(*state);
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

    // the args buffer is reinterpreted as MTL::DrawIndexedPrimitivesIndirectArguments[]
    static_assert(sizeof(RHIDrawIndexedArgs) == sizeof(MTL::DrawIndexedPrimitivesIndirectArguments));
    static_assert(offsetof(RHIDrawIndexedArgs, indexCount)    == offsetof(MTL::DrawIndexedPrimitivesIndirectArguments, indexCount));
    static_assert(offsetof(RHIDrawIndexedArgs, instanceCount) == offsetof(MTL::DrawIndexedPrimitivesIndirectArguments, instanceCount));
    static_assert(offsetof(RHIDrawIndexedArgs, firstIndex)    == offsetof(MTL::DrawIndexedPrimitivesIndirectArguments, indexStart));
    static_assert(offsetof(RHIDrawIndexedArgs, baseVertex)    == offsetof(MTL::DrawIndexedPrimitivesIndirectArguments, baseVertex));
    static_assert(offsetof(RHIDrawIndexedArgs, baseInstance)  == offsetof(MTL::DrawIndexedPrimitivesIndirectArguments, baseInstance));

    void MetalCommandList::ExecuteIndirect(const DrawBatch& batch){
        Super::ExecuteIndirect(batch);

        auto state = std::get_if<RenderPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginRenderPass()?"
        );
        CROWY_ASSERT(state->indexBuffer != nullptr);

        SetPipelineState(*batch.pso);
        flush(*state);

        auto mtlArgs = static_cast<MetalBuffer&>(*batch.args).Get();
        // no multi-draw outside indirect command buffers, so unroll batch
        for(u32 i = 0; i < batch.drawCount; ++i){
            state->encoder->drawIndexedPrimitives(
                state->topology,
                state->indexFormat,
                state->indexBuffer,
                state->indexBufferOffset,
                mtlArgs,
                batch.argsOffset + i * sizeof(RHIDrawIndexedArgs)
            );
        }
    }

    void MetalCommandList::BeginComputePass(
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        Super::BeginComputePass(textureAcquires, bufferAcquires);

        gatherWaits(textureAcquires, bufferAcquires);

        auto encoder = commandBuffer->computeCommandEncoder();
        for(const auto& wait: waitScratch){
            encoder->waitForFence(wait.fence);
        }

        passState = ComputePassState{
            .encoder = encoder
        };
    }

    void MetalCommandList::EndComputePass(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        Super::EndComputePass(textureReleases, bufferReleases);

        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginComputePass()?"
        );

        RHIBarrierSync syncBeforeUnion;
        if(const auto fence = parkReleases(
            textureReleases,
            bufferReleases,
            syncBeforeUnion
        )){
            state->encoder->updateFence(fence);
        }

        state->encoder->endEncoding();
        passState = std::monostate{};
    }

    void MetalCommandList::SetPipelineState(RHIComputePipelineState& pso){
        Super::SetPipelineState(pso);

        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginComputePass()?"
        );

        auto& metalPSO = static_cast<MetalComputePipelineState&>(pso);
        metalPSO.Bind(*state->encoder);

        state->threadsPerThreadgroup = metalPSO.GetThreadsPerThreadgroup();
    }

    void MetalCommandList::SetPushComputeConstants(
        const void* data,
        u32 size
    ){
        Super::SetPushComputeConstants(data, size);

        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginComputePass()?"
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
        Super::SetComputeConstantBuffer(buffer, slot, offset);

        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginComputePass()?"
        );
        auto mtlBuffer = static_cast<MetalBuffer&>(buffer).Get();

        state->encoder->setBuffer(
            mtlBuffer,
            offset,
            ConstantBufferSlotBase + slot
        );
    }

    void MetalCommandList::Dispatch(Size3D gridSize){
        Super::Dispatch(gridSize);

        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginComputePass()?"
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

    void MetalCommandList::DispatchBarrier(
        std::span<const RHITextureBarrier> textureBarriers,
        std::span<const RHIBufferBarrier> bufferBarriers
    ){
        Super::DispatchBarrier(textureBarriers, bufferBarriers);

        auto state = std::get_if<ComputePassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginComputePass()?"
        );

        MTL::BarrierScope scope = 0;
        if(!bufferBarriers.empty()){
            scope |= MTL::BarrierScopeBuffers;
        }
        if(!textureBarriers.empty()){
            scope |= MTL::BarrierScopeTextures;
        }
        if(scope != 0){
            state->encoder->memoryBarrier(scope);
        }
    }

    void MetalCommandList::BeginBlitPass(
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        Super::BeginBlitPass(textureAcquires, bufferAcquires);

        gatherWaits(textureAcquires, bufferAcquires);

        auto encoder = commandBuffer->blitCommandEncoder();
        for(const auto& wait: waitScratch){
            encoder->waitForFence(wait.fence);
        }

        passState = BlitPassState{
            .encoder = encoder
        };
    }

    void MetalCommandList::EndBlitPass(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        Super::EndBlitPass(textureReleases, bufferReleases);

        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlitPass()?"
        );

        RHIBarrierSync syncBeforeUnion;
        if(const auto fence = parkReleases(
            textureReleases,
            bufferReleases,
            syncBeforeUnion
        )){
            state->encoder->updateFence(fence);
        }

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
        Super::Copy(src, dst, srcOffset, dstOffset, size);

        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlitPass()?"
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
        Super::Copy(src, dst);

        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlitPass()?"
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
        Super::Copy(src, srcOffset, srcRowPitch, dst, region, mipLevel, arraySlice);

        auto state = std::get_if<BlitPassState>(&passState);
        CROWY_ASSERT(state != nullptr,
            "Did you call RHICommandList::BeginBlitPass()?"
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
