#include <array>
#include <utility>
#include <pix.h>
#include <d3dx12/d3dx12_barriers.h>
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
            .cpuDescriptor = rtvHeap.GetCPUHandle(tex->GetOrCreateRTV()),
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
            .cpuDescriptor = dsvHeap.GetCPUHandle(tex->GetOrCreateDSV()),
            .DepthBeginningAccess = beginningAccess,
            .StencilBeginningAccess = hasStencil ? beginningAccess : noBeginningAccess,
            .DepthEndingAccess = endingAccess,
            .StencilEndingAccess = hasStencil ? endingAccess : noEndingAccess
        };
    }
}

namespace Crowy
{
    DX12CommandList::DX12CommandList(
        Device& device,
        CommandQueue& commandQueue,
        RootSignature& rootSignature,
        const u64& frameIndex,
        DescriptorHeapAllocator& cbvsrvuavHeap,
        DescriptorHeapAllocator& rtvHeap,
        DescriptorHeapAllocator& dsvHeap,
        DescriptorHeapAllocator& samplerHeap
    )
        : commandQueue(commandQueue)
        , rootSignature(rootSignature)
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
        auto allocator = commandAllocators[currentIndex()];
        CHECK_HRESULT(allocator->Reset(), "Failed to reset command allocator");
        CHECK_HRESULT(commandList->Reset(
            allocator.Get(),
            nullptr
        ), "Failed to reset command list");

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
        CHECK_HRESULT(commandList->Close(),
            "Failed to close command list"
        );
    }

    void DX12CommandList::BeginRenderPass(const RHIRenderPassDesc& desc){
        CROWY_ASSERT(desc.colorAttachments.size() > 0);

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

    void DX12CommandList::EndRenderPass(){
        commandList->EndRenderPass();
    }

    void DX12CommandList::SetPipelineState(RHIGraphicsPipelineState& pso){
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
        CROWY_ASSERT(size % 4 == 0 && size < RHI_PUSH_CONSTANT_BYTES);

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
        CROWY_ASSERT(offset < buffer.GetSize());

        CROWY_ASSERT(slot < RHI_NUM_DIRECT_CBS);
        CROWY_ASSERT(offset % RHI_CB_ALIGN == 0);

        auto& dxBuffer = static_cast<DX12Buffer&>(buffer);
        auto virtualAddress = dxBuffer.GetGPUAddress() + offset;

        commandList->SetGraphicsRootConstantBufferView(
            RootParamCBBase + slot,
            virtualAddress
        );
    }

    void DX12CommandList::SetViewport(const RHIViewport& viewport){
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
        commandList->DrawIndexedInstanced(
            indexCount,
            instanceCount,
            startIndex,
            baseVertex,
            startInstance
        );
    }

    void DX12CommandList::BeginCompute() noexcept{
        CROWY_ASSERT(!inComputePass,
            "Already in a compute pass. Did you call RHICommandList::EndCompute()?"
        );
        inComputePass = true;
        currentComputePSO = nullptr;
    }

    void DX12CommandList::EndCompute() noexcept{
        CROWY_ASSERT(inComputePass,
            "Not in a compute pass. Did you call RHICommandList::BeginCompute()?"
        );
        inComputePass = false;
    }

    void DX12CommandList::SetPipelineState(RHIComputePipelineState& pso){
        CROWY_ASSERT(inComputePass,
            "Not in a compute pass. Did you call RHICommandList::BeginCompute()?"
        );
        auto& dxPso = static_cast<DX12ComputePipelineState&>(pso);
        dxPso.Bind(*commandList.Get());

        currentComputePSO = &dxPso;
    }

    void DX12CommandList::SetPushComputeConstants(
        const void* data,
        u32 size
    ){
        CROWY_ASSERT(inComputePass,
            "Not in a compute pass. Did you call RHICommandList::BeginCompute()?"
        );
        CROWY_ASSERT(size % 4 == 0 && size < RHI_PUSH_CONSTANT_BYTES);

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
        CROWY_ASSERT(inComputePass,
            "Not in a compute pass. Did you call RHICommandList::BeginCompute()?"
        );
        CROWY_ASSERT(offset < buffer.GetSize());

        CROWY_ASSERT(slot < RHI_NUM_DIRECT_CBS);
        CROWY_ASSERT(offset % RHI_CB_ALIGN == 0);

        auto& dxBuffer = static_cast<DX12Buffer&>(buffer);
        auto virtualAddress = dxBuffer.GetGPUAddress() + offset;

        commandList->SetComputeRootConstantBufferView(
            RootParamCBBase + slot,
            virtualAddress
        );
    }

    void DX12CommandList::Dispatch(Size3D gridSize){
        CROWY_ASSERT(inComputePass,
            "Not in a compute pass. Did you call RHICommandList::BeginCompute()?"
        );
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

    void DX12CommandList::BeginBlit() noexcept{
        CROWY_ASSERT(!inBlitPass,
            "Already in a blit pass. Did you call RHICommandList::EndBlit()?"
        );
        inBlitPass = true;
    }

    void DX12CommandList::EndBlit() noexcept{
        CROWY_ASSERT(inBlitPass,
            "Not in a blit pass. Did you call RHICommandList::BeginBlit()?"
        );
        inBlitPass = false;
    }

    void DX12CommandList::Copy(
        RHIBuffer& src,
        RHIBuffer& dst,
        usize srcOffset,
        usize dstOffset,
        usize size
    ){
        CROWY_ASSERT(inBlitPass,
            "Not in a blit pass. Did you call RHICommandList::BeginBlit()?"
        );
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
        CROWY_ASSERT(inBlitPass,
            "Not in a blit pass. Did you call RHICommandList::BeginBlit()?"
        );
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
        u32 mipLevel,
        u32 arraySlice
    ){
        CROWY_ASSERT(inBlitPass,
            "Not in a blit pass. Did you call RHICommandList::BeginBlit()?"
        );
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

        const D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{
            .Offset = srcOffset,
            .Footprint = {
                .Format = desc.Format,
                .Width = std::max<UINT>(1, desc.Width >> mipLevel),
                .Height = std::max<UINT>(1, desc.Height >> mipLevel),
                .Depth = 1,
                .RowPitch = srcRowPitch
            }
        };

        const CD3DX12_TEXTURE_COPY_LOCATION srcLoc(dxSrc.Get(), footprint);
        const CD3DX12_TEXTURE_COPY_LOCATION dstLoc(dxDst.Get(), subresource);

        commandList->CopyTextureRegion(
            &dstLoc,
            0, 0, 0,
            &srcLoc,
            nullptr
        );
    }

    namespace{
        inline auto convert(const RHITextureBarrier& barrier){
            auto& resource = barrier.texture;
            const auto syncAfter = barrier.syncAfter;
            const auto accessAfter = barrier.accessAfter;
            const auto layoutAfter = barrier.layoutAfter;

            return CD3DX12_TEXTURE_BARRIER(
                convert(resource.TransitionState(syncAfter)),
                convert(syncAfter),
                convert(resource.TransitionState(accessAfter)),
                convert(accessAfter),
                convert(resource.TransitionState(layoutAfter)),
                convert(layoutAfter),
                static_cast<DX12Texture&>(resource).Get(),
                CD3DX12_BARRIER_SUBRESOURCE_RANGE(0xFFFF'FFFF)
            );
        }

        inline auto convert(const RHIBufferBarrier& barrier){
            auto& resource = barrier.buffer;
            const auto syncAfter = barrier.syncAfter;
            const auto accessAfter = barrier.accessAfter;

            return CD3DX12_BUFFER_BARRIER(
                convert(resource.TransitionState(syncAfter)),
                convert(syncAfter),
                convert(resource.TransitionState(accessAfter)),
                convert(accessAfter),
                static_cast<DX12Buffer&>(resource).Get()
            );
        }
    }

    void DX12CommandList::TransitionBarrier(
        std::span<const RHITextureBarrier> textureBarriers,
        std::span<const RHIBufferBarrier> bufferBarriers
    ){
        textureBarrierScratch.reserve(textureBarriers.size());
        for(auto& barrier: textureBarriers){
            textureBarrierScratch.push_back(convert(barrier));
        }

        bufferBarrierScratch.reserve(bufferBarriers.size());
        for(auto& barrier: bufferBarriers){
            bufferBarrierScratch.push_back(convert(barrier));
        }

        const std::array barrierGroups{
            CD3DX12_BARRIER_GROUP(
                textureBarrierScratch.size(),
                textureBarrierScratch.data()
            ),
            CD3DX12_BARRIER_GROUP(
                bufferBarrierScratch.size(),
                bufferBarrierScratch.data()
            )
        };
        commandList->Barrier(
            barrierGroups.size(),
            barrierGroups.data()
        );

        textureBarrierScratch.clear();
        bufferBarrierScratch.clear();
    }

    void DX12CommandList::WaitUntilCompleted(){
        // TODO.
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
