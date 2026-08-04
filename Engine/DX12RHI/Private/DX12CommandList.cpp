#include <array>
#include <utility>
#include <pix.h>
#include <d3dx12/d3dx12_barriers.h>
#include <winscard.h>
#include "Assert.hpp"
#include "DX12Definitions.hpp"
#include "DescriptorHeapAllocator.hpp"
#include "DX12Buffer.hpp"
#include "DX12CommandList.hpp"
#include "DX12PipelineState.hpp"
#include "DX12Texture.hpp"
#include "DX12Util.hpp"
#include "IntMath.hpp"
#include "RHIDefinitions.hpp"

namespace{
    D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE convert(Crowy::RHILoadAction action){
        using enum Crowy::RHILoadAction;

        switch(action){
        case Load:     return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        case Clear:    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        case DontCare: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        default:
            std::unreachable();
        }
    }

    D3D12_RENDER_PASS_ENDING_ACCESS_TYPE convert(Crowy::RHIStoreAction action){
        using enum Crowy::RHIStoreAction;

        switch(action){
        case Store:    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        case DontCare: return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        default:
            std::unreachable();
        }
    }

    auto convert(
        Crowy::RHIColorAttachment desc,
        Crowy::DescriptorHeapAllocator& rtvHeap
    ){
        using namespace Crowy;

        auto tex = static_cast<DX12Texture*>(desc.texture);

        return D3D12_RENDER_PASS_RENDER_TARGET_DESC{
            .cpuDescriptor = rtvHeap.GetCPUHandle(
                tex->GetOrCreateRTV(desc.mipLevel)
            ),
            .BeginningAccess = {
                .Type = convert(desc.loadAction),
                .Clear = {
                    .ClearValue = {
                        .Format = convert(tex->GetFormat()),
                        .Color = {
                            desc.clearColor.x,
                            desc.clearColor.y,
                            desc.clearColor.z,
                            desc.clearColor.w
                        }
                    }
                }
            },
            .EndingAccess = {
                .Type = convert(desc.storeAction)
            }
        };
    }

    auto convert(
        Crowy::RHIDepthAttachment desc,
        Crowy::DescriptorHeapAllocator& dsvHeap
    ){
        using namespace Crowy;

        auto tex = static_cast<DX12Texture*>(desc.texture);
        const auto format = tex->GetFormat();

        const D3D12_RENDER_PASS_BEGINNING_ACCESS beginningAccess{
            .Type = convert(desc.loadAction),
            .Clear = {
                .ClearValue = {
                    .Format = convert(format),
                    .DepthStencil = {
                        .Depth = desc.clearDepthStencil.depth,
                        .Stencil = desc.clearDepthStencil.stencil
                    }
                }
            }
        };
        const D3D12_RENDER_PASS_ENDING_ACCESS endingAccess{
            .Type = convert(desc.storeAction)
        };
        // a depth-only view rejects the pass outright if the stencil access
        // claims it will clear or discard something that is not there
        const D3D12_RENDER_PASS_BEGINNING_ACCESS noBeginningAccess{
            .Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS
        };
        const D3D12_RENDER_PASS_ENDING_ACCESS noEndingAccess{
            .Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS
        };
        const bool hasStencil = HasStencil(format);

        return D3D12_RENDER_PASS_DEPTH_STENCIL_DESC{
            .cpuDescriptor = dsvHeap.GetCPUHandle(
                tex->GetOrCreateDSV(desc.mipLevel)
            ),
            .DepthBeginningAccess = beginningAccess,
            .StencilBeginningAccess = hasStencil ? beginningAccess : noBeginningAccess,
            .DepthEndingAccess = endingAccess,
            .StencilEndingAccess = hasStencil ? endingAccess : noEndingAccess
        };
    }
}

namespace Crowy
{
    // stateless §5 rules live in the RHICommandList base now;
    // this backend only keeps what its pending maps can see
    namespace{
        inline auto convert(const RHISubresourceRange& range){
            if(range.numMips == 0){
                // flat subresource index, RHI_ALL_SUBRESOURCES for everything
                return CD3DX12_BARRIER_SUBRESOURCE_RANGE(range.firstMip);
            }

            // planes are left at the default single plane
            return CD3DX12_BARRIER_SUBRESOURCE_RANGE(
                range.firstMip,
                range.numMips,
                range.firstArraySlice,
                range.numArraySlice
            );
        }
    }

    DX12CommandList::DX12CommandList(
        Device& device,
        CommandQueue& commandQueue,
        RootSignature& rootSignature,
        CommandSignature& drawIndexedSignature,
        const u64& frameIndex,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        DescriptorHeapAllocator& samplerHeap
    )
        : commandQueue(commandQueue)
        , rootSignature(rootSignature)
        , drawIndexedSignature(drawIndexedSignature)
        , frameIndex(frameIndex)
        , cbvsrvuavHeap(cbvsrvuavHeap)
        , rtvHeap(rtvHeap)
        , dsvHeap(dsvHeap)
        , samplerHeap(samplerHeap)
    {
        for(u32 i = 0; i < RHI_FRAMES_IN_FLIGHT; ++i){
            CHECK_HRESULT(device.CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&commandAllocators[i])
            ), "Failed to create command allocator");
        }

        CHECK_HRESULT(device.CreateCommandList1(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&commandList)
        ), "Failed to create command list");
    }

    DX12CommandList::~DX12CommandList() = default;

    void DX12CommandList::Begin(){
        Super::Begin();

        auto allocator = commandAllocators[currentIndex()];
        CHECK_HRESULT(allocator->Reset(), "Failed to reset command allocator");
        CHECK_HRESULT(commandList->Reset(
            allocator.Get(),
            nullptr
        ), "Failed to reset command list");

        pendingTextureReleases.clear();
        pendingBufferReleases.clear();

        std::array heaps{
            cbvsrvuavHeap.Get(),
            samplerHeap.Get()
        };
        commandList->SetDescriptorHeaps(
            heaps.size(),
            heaps.data()
        );

        commandList->SetGraphicsRootSignature(&rootSignature);
        commandList->SetComputeRootSignature(&rootSignature);
    }

    void DX12CommandList::Close(){
        Super::Close();

        flushPendingReleases();

        CHECK_HRESULT(commandList->Close(),
            "Failed to close command list"
        );
    }

    void DX12CommandList::BeginRenderPass(
        const RHIRenderPassDesc& desc,
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        Super::BeginRenderPass(desc, textureAcquires, bufferAcquires);

        CROWY_ASSERT(desc.colorAttachments.size() > 0);

        // barriers are illegal inside a D3D12 render pass,
        // so the acquires land right before it
        applyAcquires(textureAcquires, bufferAcquires);

        std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, RHI_MAX_RENDER_TARGETS> rts;
        for(usize i=0; i<desc.colorAttachments.size(); ++i)
            rts[i] = ::convert(desc.colorAttachments[i], rtvHeap);

        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dt;
        if(desc.depthAttachment.has_value())
            dt = ::convert(*desc.depthAttachment, dsvHeap);

        commandList->BeginRenderPass(
            desc.colorAttachments.size(),
            rts.data(),
            desc.depthAttachment.has_value() ?
                &dt :
                nullptr,
            D3D12_RENDER_PASS_FLAG_NONE
        );
    }

    void DX12CommandList::EndRenderPass(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        Super::EndRenderPass(textureReleases, bufferReleases);

        commandList->EndRenderPass();

        queueReleases(textureReleases, bufferReleases);
    }

    void DX12CommandList::SetPipelineState(RHIGraphicsPipelineState& pso){
        Super::SetPipelineState(pso);

        auto& dxPso = static_cast<DX12GraphicsPipelineState&>(pso);
        dxPso.Bind(*commandList.Get());

        currentGraphicsPSO = &dxPso;
    }

    void DX12CommandList::SetVertexBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 stride,
        u32 offset
    ){
        Super::SetVertexBuffer(buffer, slot, stride, offset);

        auto& dxBuffer = static_cast<DX12Buffer&>(buffer);
        const std::array vbViews{
            D3D12_VERTEX_BUFFER_VIEW{
                .BufferLocation = dxBuffer.GetGPUAddress() + offset,
                .SizeInBytes = static_cast<UINT>(dxBuffer.GetSize() - offset),
                .StrideInBytes = stride
            }
        };

        commandList->IASetVertexBuffers(
            slot,
            vbViews.size(),
            vbViews.data()
        );
    }

    void DX12CommandList::SetIndexBuffer(
        RHIBuffer& buffer,
        RHIIndexFormat format,
        u32 offset
    ){
        Super::SetIndexBuffer(buffer, format, offset);

        auto& dxBuffer = static_cast<DX12Buffer&>(buffer);
        const D3D12_INDEX_BUFFER_VIEW ibView{
            .BufferLocation = dxBuffer.GetGPUAddress() + offset,
            .SizeInBytes = static_cast<UINT>(dxBuffer.GetSize() - offset),
            .Format = RHIIndexFormat::UInt32 == format ?
                DXGI_FORMAT_R32_UINT :
                DXGI_FORMAT_R16_UINT
        };

        commandList->IASetIndexBuffer(
            &ibView
        );
    }

    void DX12CommandList::SetPushGraphicsConstants(
        const void* data,
        u32 size
    ){
        Super::SetPushGraphicsConstants(data, size);

        commandList->SetGraphicsRoot32BitConstants(
            RootParamPush,
            size / 4,
            data,
            0
        );
    }

    void DX12CommandList::SetGraphicsConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 offset
    ){
        Super::SetGraphicsConstantBuffer(buffer, slot, offset);

        auto& dxBuffer = static_cast<DX12Buffer&>(buffer);
        auto virtualAddress = dxBuffer.GetGPUAddress() + offset;

        commandList->SetGraphicsRootConstantBufferView(
            RootParamCBBase + slot,
            virtualAddress
        );
    }

    void DX12CommandList::SetViewport(const RHIViewport& viewport){
        Super::SetViewport(viewport);

        const std::array vps{
            D3D12_VIEWPORT{
                .TopLeftX = viewport.x,
                .TopLeftY = viewport.y,
                .Width = viewport.width,
                .Height = viewport.height,
                .MinDepth = viewport.minDepth,
                .MaxDepth = viewport.maxDepth
            }
        };
        commandList->RSSetViewports(
            vps.size(),
            vps.data()
        );
    }

    void DX12CommandList::SetScissorRect(const RHIScissorRect& scissor){
        Super::SetScissorRect(scissor);

        const std::array rects{
            D3D12_RECT{
                .left = scissor.left,
                .top = scissor.top,
                .right = scissor.right,
                .bottom = scissor.bottom
            }
        };
        commandList->RSSetScissorRects(
            rects.size(),
            rects.data()
        );
    }

    void DX12CommandList::Draw(
        u32 vertexCount,
        u32 instanceCount,
        u32 startVertex,
        u32 startInstance
    ){
        Super::Draw(vertexCount, instanceCount, startVertex, startInstance);

        commandList->DrawInstanced(
            vertexCount,
            instanceCount,
            startVertex,
            startInstance
        );
    }

    void DX12CommandList::DrawIndexed(
        u32 indexCount,
        u32 instanceCount,
        u32 startIndex,
        i32 baseVertex,
        u32 startInstance
    ){
        Super::DrawIndexed(indexCount, instanceCount, startIndex, baseVertex, startInstance);

        commandList->DrawIndexedInstanced(
            indexCount,
            instanceCount,
            startIndex,
            baseVertex,
            startInstance
        );
    }

    // the args buffer is reinterpreted as D3D12_DRAW_INDEXED_ARGUMENTS[]
    static_assert(sizeof(RHIDrawIndexedArgs) == sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    static_assert(offsetof(RHIDrawIndexedArgs, indexCount)    == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, IndexCountPerInstance));
    static_assert(offsetof(RHIDrawIndexedArgs, instanceCount) == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, InstanceCount));
    static_assert(offsetof(RHIDrawIndexedArgs, firstIndex)    == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartIndexLocation));
    static_assert(offsetof(RHIDrawIndexedArgs, baseVertex)    == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, BaseVertexLocation));
    static_assert(offsetof(RHIDrawIndexedArgs, baseInstance)  == offsetof(D3D12_DRAW_INDEXED_ARGUMENTS, StartInstanceLocation));

    void DX12CommandList::ExecuteIndirect(const DrawBatch& batch){
        Super::ExecuteIndirect(batch);

        SetPipelineState(*batch.pso);

        auto& dxArgs = static_cast<DX12Buffer&>(*batch.args);
        commandList->ExecuteIndirect(
            &drawIndexedSignature,
            batch.drawCount,
            dxArgs.Get(),
            batch.argsOffset,
            nullptr,
            0
        );
    }

    void DX12CommandList::BeginComputePass(
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        Super::BeginComputePass(textureAcquires, bufferAcquires);

        applyAcquires(textureAcquires, bufferAcquires);

        currentComputePSO = nullptr;
    }

    void DX12CommandList::EndComputePass(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        Super::EndComputePass(textureReleases, bufferReleases);

        queueReleases(textureReleases, bufferReleases);
    }

    void DX12CommandList::SetPipelineState(RHIComputePipelineState& pso){
        Super::SetPipelineState(pso);

        auto& dxPso = static_cast<DX12ComputePipelineState&>(pso);
        dxPso.Bind(*commandList.Get());

        currentComputePSO = &dxPso;
    }

    void DX12CommandList::SetPushComputeConstants(
        const void* data,
        u32 size
    ){
        Super::SetPushComputeConstants(data, size);

        commandList->SetComputeRoot32BitConstants(
            RootParamPush,
            size / 4,
            data,
            0
        );
    }

    void DX12CommandList::SetComputeConstantBuffer(
        RHIBuffer& buffer,
        u32 slot,
        u32 offset
    ){
        Super::SetComputeConstantBuffer(buffer, slot, offset);

        auto& dxBuffer = static_cast<DX12Buffer&>(buffer);
        auto virtualAddress = dxBuffer.GetGPUAddress() + offset;

        commandList->SetComputeRootConstantBufferView(
            RootParamCBBase + slot,
            virtualAddress
        );
    }

    void DX12CommandList::Dispatch(Size3D gridSize){
        Super::Dispatch(gridSize);

        CROWY_ASSERT(currentComputePSO != nullptr,
            "Did you call RHICommandList::SetPipelineState(ComputePSO)?"
        );
        auto threadGroupSize = currentComputePSO->getThreadGroupSize();

        commandList->Dispatch(
            ceilDiv(gridSize.x, threadGroupSize.x),
            ceilDiv(gridSize.y, threadGroupSize.y),
            ceilDiv(gridSize.z, threadGroupSize.z)
        );
    }

    void DX12CommandList::BeginBlitPass(
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        Super::BeginBlitPass(textureAcquires, bufferAcquires);

        applyAcquires(textureAcquires, bufferAcquires);
    }

    void DX12CommandList::EndBlitPass(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        Super::EndBlitPass(textureReleases, bufferReleases);

        queueReleases(textureReleases, bufferReleases);
    }

    void DX12CommandList::DispatchBarrier(
        std::span<const RHITextureBarrier> textureBarriers,
        std::span<const RHIBufferBarrier> bufferBarriers
    ){
        Super::DispatchBarrier(textureBarriers, bufferBarriers);

        for(const auto& barrier: textureBarriers){
            pushFused(barrier);
        }
        for(const auto& barrier: bufferBarriers){
            pushFused(barrier);
        }

        flushBarrierScratch();
    }

    void DX12CommandList::Copy(
        RHIBuffer& src,
        RHIBuffer& dst,
        usize srcOffset,
        usize dstOffset,
        usize size
    ){
        Super::Copy(src, dst, srcOffset, dstOffset, size);

        auto& dxSrc = static_cast<DX12Buffer&>(src);
        auto& dxDst = static_cast<DX12Buffer&>(dst);

        commandList->CopyBufferRegion(
            dxDst.Get(),
            dstOffset,
            dxSrc.Get(),
            srcOffset,
            size
        );
    }

    void DX12CommandList::Copy(
        RHITexture& src,
        RHITexture& dst
    ){
        Super::Copy(src, dst);

        commandList->CopyResource(
            static_cast<DX12Texture&>(dst).Get(),
            static_cast<DX12Texture&>(src).Get()
        );
    }

    void DX12CommandList::Copy(
        RHIBuffer& src,
        u64 srcOffset,
        u32 srcRowPitch,
        RHITexture& dst,
        const RHITextureRegion& region,
        u32 mipLevel,
        u32 arraySlice
    ){
        Super::Copy(src, srcOffset, srcRowPitch, dst, region, mipLevel, arraySlice);

        auto& dxSrc = static_cast<DX12Buffer&>(src);
        auto& dxDst = static_cast<DX12Texture&>(dst);

        const auto desc = dxDst.Get()->GetDesc();
        const auto subresource = D3D12CalcSubresource(
            mipLevel,
            arraySlice,
            0,
            desc.MipLevels,
            desc.DepthOrArraySize
        );

        CROWY_ASSERT(
            region.x + region.width <= dxDst.GetWidth(mipLevel) &&
            region.y + region.height <= dxDst.GetHeight(mipLevel),
            "copy region reaches past the mip"
        );

        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{
            .Offset = srcOffset,
            .Footprint = {
                .Format = desc.Format,
                .Width = region.width,
                .Height = region.height,
                .Depth = 1,
                .RowPitch = srcRowPitch
            }
        };

        const CD3DX12_TEXTURE_COPY_LOCATION srcLoc(dxSrc.Get(), footprint);
        const CD3DX12_TEXTURE_COPY_LOCATION dstLoc(dxDst.Get(), subresource);

        commandList->CopyTextureRegion(
            &dstLoc,
            region.x, region.y, 0,
            &srcLoc,
            nullptr
        );
    }

    void DX12CommandList::pushFused(const RHITextureBarrier& barrier){
        textureBarrierScratch.push_back(CD3DX12_TEXTURE_BARRIER(
            convert(barrier.syncBefore),
            convert(barrier.syncAfter),
            convert(barrier.accessBefore),
            convert(barrier.accessAfter),
            convert(barrier.layoutBefore),
            convert(barrier.layoutAfter),
            static_cast<DX12Texture*>(barrier.texture)->Get(),
            convert(barrier.range),
            barrier.discard ?
                D3D12_TEXTURE_BARRIER_FLAG_DISCARD :
                D3D12_TEXTURE_BARRIER_FLAG_NONE
        ));
    }

    void DX12CommandList::pushFused(const RHIBufferBarrier& barrier){
        bufferBarrierScratch.push_back(CD3DX12_BUFFER_BARRIER(
            convert(barrier.syncBefore),
            convert(barrier.syncAfter),
            convert(barrier.accessBefore),
            convert(barrier.accessAfter),
            static_cast<DX12Buffer*>(barrier.buffer)->Get()
        ));
    }

    void DX12CommandList::flushBarrierScratch(){
        if(textureBarrierScratch.empty() && bufferBarrierScratch.empty()){
            return;
        }

        std::array<D3D12_BARRIER_GROUP, 2> barrierGroups;
        u32 groupCount = 0;
        if(!textureBarrierScratch.empty()){
            barrierGroups[groupCount++] = CD3DX12_BARRIER_GROUP(
                textureBarrierScratch.size(),
                textureBarrierScratch.data()
            );
        }
        if(!bufferBarrierScratch.empty()){
            barrierGroups[groupCount++] = CD3DX12_BARRIER_GROUP(
                bufferBarrierScratch.size(),
                bufferBarrierScratch.data()
            );
        }
        commandList->Barrier(
            groupCount,
            barrierGroups.data()
        );

        textureBarrierScratch.clear();
        bufferBarrierScratch.clear();
    }

    void DX12CommandList::queueRelease(const RHITextureBarrier& barrier){
        if(barrier.syncAfter == RHIBarrierSync::None){
            // self-contained release (e.g. RenderTarget → Present):
            // what follows is guaranteed elsewhere, so fuse on the spot
            pushFused(barrier);
            return;
        }

        auto& pending = pendingTextureReleases[barrier.texture];
        for(auto& entry: pending){
            if(entry.barrier.range == barrier.range){
                // re-release of the same range = new producer. if the old edge
                // was never acquired its transition is still unrecorded, so
                // complete it here to keep the layout timeline coherent
                if(!entry.consumed){
                    pushFused(entry.barrier);
                }
                entry = {.barrier = barrier};
                return;
            }
        }
        pending.push_back({.barrier = barrier});
    }

    void DX12CommandList::queueRelease(const RHIBufferBarrier& barrier){
        if(barrier.syncAfter == RHIBarrierSync::None){
            pushFused(barrier);
            return;
        }

        const auto [it, inserted] = pendingBufferReleases.try_emplace(
            barrier.buffer,
            PendingRelease<RHIBufferBarrier>{.barrier = barrier}
        );
        if(!inserted){
            if(!it->second.consumed){
                pushFused(it->second.barrier);
            }
            it->second = {.barrier = barrier};
        }
    }

    void DX12CommandList::queueReleases(
        std::span<const RHITextureBarrier> textureReleases,
        std::span<const RHIBufferBarrier> bufferReleases
    ){
        for(const auto& release: textureReleases){
            queueRelease(release);
        }
        for(const auto& release: bufferReleases){
            queueRelease(release);
        }
        // self-contained releases recorded above land here
        flushBarrierScratch();
    }

    void DX12CommandList::applyAcquire(const RHITextureBarrier& barrier){
        if(const auto it = pendingTextureReleases.find(barrier.texture);
            it != pendingTextureReleases.end()
        ){
            for(auto& entry: it->second){
                // full-edge equality doubles as the §5 cross validation:
                // both halves carried the same value or they do not pair
                if(entry.barrier == barrier){
                    // extra consumers of an identical edge need no barrier in
                    // the fuse strategy: sync scopes are queue-global, so the
                    // first fused barrier already orders every later command
                    // in the edge's consumer stages, and it also finished the
                    // access flush and layout transition. equality matching
                    // guarantees this consumer's stages are the same.
                    if(!entry.consumed){
                        entry.consumed = true;
                        pushFused(barrier);
                    }
                    return;
                }
            }
        }

        CROWY_ASSERT(
            barrier.syncBefore == RHIBarrierSync::None || barrier.crossSubmission,
            "acquire with real before-sync has no matching release in this "
            "command list; producers in earlier submissions need the "
            "crossSubmission flag (MakeCrossSubmissionBarrier)"
        );
        pushFused(barrier);
    }

    void DX12CommandList::applyAcquire(const RHIBufferBarrier& barrier){
        if(const auto it = pendingBufferReleases.find(barrier.buffer);
            it != pendingBufferReleases.end() && it->second.barrier == barrier
        ){
            // see the texture path: identical extra consumers ride the first
            // fused barrier's queue-global sync scope
            auto& entry = it->second;
            if(!entry.consumed){
                entry.consumed = true;
                pushFused(barrier);
            }
            return;
        }

        CROWY_ASSERT(
            barrier.syncBefore == RHIBarrierSync::None || barrier.crossSubmission,
            "acquire with real before-sync has no matching release in this "
            "command list; producers in earlier submissions need the "
            "crossSubmission flag (MakeCrossSubmissionBarrier)"
        );
        pushFused(barrier);
    }

    void DX12CommandList::applyAcquires(
        std::span<const RHITextureBarrier> textureAcquires,
        std::span<const RHIBufferBarrier> bufferAcquires
    ){
        for(const auto& acquire: textureAcquires){
            applyAcquire(acquire);
        }
        for(const auto& acquire: bufferAcquires){
            applyAcquire(acquire);
        }
        flushBarrierScratch();
    }

    void DX12CommandList::flushPendingReleases(){
        // releases nobody acquired complete here as single barriers - the
        // hand-off point for consumers living in later submissions
        // (e.g. resources uploaded at creation time).
        // ordering across submissions is queue-level,
        // so finishing the transition at Close is enough.
        for(auto& [_, entries]: pendingTextureReleases){
            for(auto& entry: entries){
                if(!entry.consumed){
                    pushFused(entry.barrier);
                }
            }
        }
        for(auto& [_, entry]: pendingBufferReleases){
            if(!entry.consumed){
                pushFused(entry.barrier);
            }
        }
        pendingTextureReleases.clear();
        pendingBufferReleases.clear();

        flushBarrierScratch();
    }

    void DX12CommandList::BeginEvent(CStr name){
        PIXBeginEvent(
            commandList.Get(),
            PIX_COLOR(0xFF, 0, 0),
            name
        );
    }

    void DX12CommandList::EndEvent(){
        PIXEndEvent(
            commandList.Get()
        );
    }

    void DX12CommandList::SetMarker(CStr name){
        PIXSetMarker(
            commandList.Get(),
            PIX_COLOR(0xFF, 0, 0),
            name
        );
    }
}
