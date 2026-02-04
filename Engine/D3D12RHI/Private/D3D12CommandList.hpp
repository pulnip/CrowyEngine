#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <d3d12.h>
#include "assert.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif
#include "D3D12Buffer.hpp"
#include "D3D12Fence.hpp"
#include "D3D12PipelineState.hpp"
#include "D3D12Sampler.hpp"
#include "D3D12Swapchain.hpp"
#include "D3D12Texture.hpp"

namespace Crowy
{
    static D3D_PRIMITIVE_TOPOLOGY convert(RHIPrimitiveTopology topology){
        switch(topology){
        case RHIPrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case RHIPrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case RHIPrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case RHIPrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case RHIPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            std::unreachable();
        }
    }

    static D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE convert(RHILoadAction action){
        switch(action){
        case RHILoadAction::Load:     return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        case RHILoadAction::Clear:    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        case RHILoadAction::DontCare: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        default:
            std::unreachable();
        }
    }

    static D3D12_RENDER_PASS_ENDING_ACCESS_TYPE convert(RHIStoreAction action){
        switch(action){
        case RHIStoreAction::Store:    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        case RHIStoreAction::DontCare: return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        default:
            std::unreachable();
        }
    }

    class D3D12CommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
    private:
        ID3D12CommandQueue* commandQueue = nullptr;
        ID3D12GraphicsCommandList4* commandList = nullptr;
        ID3D12CommandAllocator* commandAllocators[RHI_FRAMES_IN_FLIGHT];
        int currentIndex = 0;
        DescriptorHeapAllocator* cbv_srvHeap = nullptr;
        DescriptorHeapAllocator* rtvHeap = nullptr;
        DescriptorHeapAllocator* dsvHeap = nullptr;
        DescriptorHeapAllocator* samplerHeap = nullptr;

        std::vector<D3D12Buffer*> perFrameBuffers;

        bool isRecording = false;

    public:
        D3D12CommandList(
            ID3D12Device4* device,
            ID3D12CommandQueue* commandQueue,
            DescriptorHeapAllocator* cbv_srvHeap,
            DescriptorHeapAllocator* rtvHeap,
            DescriptorHeapAllocator* dsvHeap,
            DescriptorHeapAllocator* samplerHeap
        )
            : commandQueue(commandQueue)
            , cbv_srvHeap(cbv_srvHeap), rtvHeap(rtvHeap)
            , dsvHeap(dsvHeap), samplerHeap(samplerHeap)
        {
            for(int i = 0; i < RHI_FRAMES_IN_FLIGHT; ++i){
                if(FAILED(device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&commandAllocators[i])
                ))){
                    throw std::runtime_error("Failed to create command allocator");
                }
            }

            if(FAILED(device->CreateCommandList1(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                D3D12_COMMAND_LIST_FLAG_NONE,
                IID_PPV_ARGS(&commandList)
            ))){
                throw std::runtime_error("Failed to create command list");
            }
        }

        ~D3D12CommandList(){
            if(commandList != nullptr){
                commandList->Release();
                commandList = nullptr;
            }
            for(int i = 0; i < RHI_FRAMES_IN_FLIGHT; ++i){
                if(commandAllocators[i] != nullptr){
                    commandAllocators[i]->Release();
                    commandAllocators[i] = nullptr;
                }
            }
        }

        void begin() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(!isRecording,
                "Did you call RHICommandList::close()?"
            );

            auto allocator = commandAllocators[currentIndex];

            // how to gurantee allocator's work done ??
            allocator->Reset();
            commandList->Reset(allocator, nullptr);

            ID3D12DescriptorHeap* heaps[] = {
                cbv_srvHeap->get(),
                rtvHeap->get(),
                dsvHeap->get(),
                samplerHeap->get()
            };
            commandList->SetDescriptorHeaps(_countof(heaps), heaps);

            isRecording = true;
        }

        void flush() noexcept RHI_OVERRIDE{

        }

        void close() noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isRecording,
                "Did you call RHICommandList::begin()?"
            );

            commandList->Close();

            ID3D12CommandList* cmdLists[] = {commandList};
            commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

            for(auto& buffer: perFrameBuffers){
                buffer->swap();
            }
            currentIndex = (currentIndex + 1) % RHI_FRAMES_IN_FLIGHT;

            isRecording = false;
        }

        void reset() noexcept RHI_OVERRIDE{
            if(isRecording){
                flush();

                isRecording = false;
            }
        }

        void beginRenderPass(
            std::span<RHITexture*> renderTargets,
            RHITexture* depthTarget,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(renderTargets.size() > 0);

            std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtDescs;
            rtDescs.reserve(renderTargets.size());
            for(size_t i = 0; i < renderTargets.size(); ++i){
                const auto& renderTarget = static_cast<D3D12Texture&>(*renderTargets[i]);
                D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDesc{
                    .cpuDescriptor = rtvHeap->getCPUHandle(renderTarget.getRTVHeapIndex()),
                    .BeginningAccess = {.Type = convert(loadAction)},
                    .EndingAccess = {.Type = convert(storeAction)}
                };
                if(loadAction == RHILoadAction::Clear){
                    rtDesc.BeginningAccess.Clear.ClearValue = {
                        .Format = convert(renderTarget.getFormat()),
                        .Color = {
                            clearColor.r, clearColor.g, clearColor.b, clearColor.a
                        }
                    };
                }

                rtDescs.push_back(std::move(rtDesc));
            }

            beginRenderPass(
                rtDescs,
                static_cast<D3D12Texture*>(depthTarget),
                loadAction,
                storeAction,
                clearColor,
                clearDS
            );
        }

        void beginRenderPass(
            RHISwapchain& swapchain,
            RHITexture* depthTarget,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS,
            const char* debugName
        ) noexcept RHI_OVERRIDE{
            auto& d3dSwapchain = static_cast<D3D12Swapchain&>(swapchain);

            D3D12_RENDER_PASS_RENDER_TARGET_DESC rtDescs[] = {
                {
                    .cpuDescriptor = rtvHeap->getCPUHandle(d3dSwapchain.getRTVHeapIndex()),
                    .BeginningAccess = {.Type = convert(loadAction)},
                    .EndingAccess = {.Type = convert(storeAction)}
                }
            };
            if(loadAction == RHILoadAction::Clear){
                rtDescs[0].BeginningAccess.Clear.ClearValue = {
                    .Format = d3dSwapchain.getFormat(),
                    .Color = {clearColor.r, clearColor.g, clearColor.b, clearColor.a}
                };
            }

            beginRenderPass(
                rtDescs,
                static_cast<D3D12Texture*>(depthTarget),
                loadAction,
                storeAction,
                clearColor,
                clearDS
            );
        }

        void endRenderPass() noexcept RHI_OVERRIDE{
            commandList->EndRenderPass();


            // // Transition swapchain back to present if needed
            // if(currentRenderTarget){
            //     transitionResource(currentRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            // }
        }

        void setPipelineState(RHIPipelineState* pso) noexcept RHI_OVERRIDE{
            auto d3dPSO = static_cast<D3D12PipelineState*>(pso);

            commandList->IASetPrimitiveTopology(convert(d3dPSO->getTopology()));
            commandList->SetGraphicsRootSignature(d3dPSO->getRootSignature());
            commandList->SetPipelineState(d3dPSO->getPipeline());
        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto& d3dBuffer = static_cast<D3D12Buffer&>(buffer);

            D3D12_VERTEX_BUFFER_VIEW vbView{
                .BufferLocation = d3dBuffer.getGPUAddress() + offset,
                .SizeInBytes = static_cast<UINT>(d3dBuffer.getSize() - offset),
                .StrideInBytes = stride
            };

            commandList->IASetVertexBuffers(slot, 1, &vbView);
        }

        void setIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto& d3dBuffer = static_cast<D3D12Buffer&>(buffer);

            D3D12_INDEX_BUFFER_VIEW ibView{
                .BufferLocation = d3dBuffer.getGPUAddress() + offset,
                .SizeInBytes = static_cast<UINT>(d3dBuffer.getSize() - offset),
                .Format = RHIIndexFormat::UInt32 == format ?
                    DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT
            };

            commandList->IASetIndexBuffer(&ibView);
        }

        void setConstantBuffer(
            RHIShaderStage stage,
            uint32_t slot,
            RHIBuffer& buffer,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            auto& d3dBuffer = static_cast<D3D12Buffer&>(buffer);
            auto cbvAddress = d3dBuffer.getGPUAddress();

            switch(stage){
            case RHIShaderStage::VertexShader:
                [[fallthrough]];
            case RHIShaderStage::FragmentShader:
                commandList->SetGraphicsRootConstantBufferView(UINT_MAX, cbvAddress);
                break;
            case RHIShaderStage::ComputeShader:
                commandList->SetComputeRootConstantBufferView(UINT_MAX, cbvAddress);
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
            auto& d3dTexture = static_cast<D3D12Texture&>(texture);
            auto handle = cbv_srvHeap->getGPUHandle(d3dTexture.getSRVHeapIndex());

            switch(stage){
            case RHIShaderStage::VertexShader:
                [[fallthrough]];
            case RHIShaderStage::FragmentShader:
                commandList->SetGraphicsRootDescriptorTable(UINT_MAX, handle);
                break;
            case RHIShaderStage::ComputeShader:
                commandList->SetComputeRootDescriptorTable(UINT_MAX, handle);
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
            auto& d3dBuffer = static_cast<D3D12Buffer&>(buffer);
            auto srvAddress = d3dBuffer.getGPUAddress();

            switch(stage){
            case RHIShaderStage::VertexShader:
                [[fallthrough]];
            case RHIShaderStage::FragmentShader:
                commandList->SetGraphicsRootShaderResourceView(UINT_MAX, srvAddress);
                break;
            case RHIShaderStage::ComputeShader:
                commandList->SetComputeRootShaderResourceView(UINT_MAX, srvAddress);
                break;
            default:
                std::unreachable();
            }
        }

        void setSampler(
            uint32_t slot,
            RHISampler& sampler,
            RHIShaderStage stage
        ) noexcept RHI_OVERRIDE{
            auto& d3dSampler = static_cast<D3D12Sampler&>(sampler);
            auto handle = samplerHeap->getGPUHandle(d3dSampler.getHeapIndex());

            switch(stage){
            case RHIShaderStage::VertexShader:
                [[fallthrough]];
            case RHIShaderStage::FragmentShader:
                commandList->SetGraphicsRootDescriptorTable(UINT_MAX, handle);
                break;
            case RHIShaderStage::ComputeShader:
                commandList->SetComputeRootDescriptorTable(UINT_MAX, handle);
                break;
            default:
                std::unreachable();
            }
        }

        void setViewport(const RHIViewport& viewport) noexcept RHI_OVERRIDE{
            D3D12_VIEWPORT vp{
                .TopLeftX = viewport.x,
                .TopLeftY = viewport.y,
                .Width = viewport.width,
                .Height = viewport.height,
                .MinDepth = viewport.minDepth,
                .MaxDepth = viewport.maxDepth
            };
            commandList->RSSetViewports(1, &vp);
        }

        void setScissorRect(const RHIScissorRect& scissor) noexcept RHI_OVERRIDE{
            D3D12_RECT rect{
                .left = scissor.left,
                .top = scissor.top,
                .right = scissor.right,
                .bottom = scissor.bottom
            };

            commandList->RSSetScissorRects(1, &rect);
        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) noexcept RHI_OVERRIDE{
            commandList->DrawInstanced(
                vertexCount,
                instanceCount,
                startVertex,
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
            commandList->DrawIndexedInstanced(
                indexCount,
                instanceCount,
                startIndex,
                baseVertex,
                startInstance
            );
        }

        void dispatch(
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY,
            uint32_t threadGroupCountZ
        ) noexcept RHI_OVERRIDE{
            commandList->Dispatch(
                threadGroupCountX,
                threadGroupCountY,
                threadGroupCountZ
            );
        }

        void transitionBarrier(
            RHITexture& texture,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{
            RHIResourceState before = texture.getState();
            if(before == after) return;

            auto resource = static_cast<D3D12Texture&>(texture).get();
            transitionResource(resource, convert(before), convert(after));

            texture.setState(after);
        }

        void transitionBarrier(
            RHIBuffer& buffer,
            RHIResourceState after
        ) noexcept RHI_OVERRIDE{
            RHIResourceState before = buffer.getState();
            if(before == after) return;

            auto resource = static_cast<D3D12Buffer&>(buffer).get();
            transitionResource(resource, convert(before), convert(after));

            buffer.setState(after);
        }

        void uavBarrier(RHITexture& texture) noexcept RHI_OVERRIDE{
            auto resource = static_cast<D3D12Texture&>(texture).get();
            D3D12_RESOURCE_BARRIER barrier{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .UAV = {
                    .pResource = resource
                }
            };

            commandList->ResourceBarrier(1, &barrier);
        }

        void uavBarrier(RHIBuffer& buffer) noexcept RHI_OVERRIDE{
            auto resource = static_cast<D3D12Buffer&>(buffer).get();
            D3D12_RESOURCE_BARRIER barrier{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
                .UAV = {
                    .pResource = resource
                }
            };

            commandList->ResourceBarrier(1, &barrier);
        }

        void signalFence(RHIFence& fence, uint64_t value) noexcept RHI_OVERRIDE{
            auto& d3dFence = static_cast<D3D12Fence&>(fence);

        }

        void waitFence(RHIFence& fence, uint64_t value) noexcept RHI_OVERRIDE{
            auto& d3dFence = static_cast<D3D12Fence&>(fence);

        }

        void copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) noexcept RHI_OVERRIDE{
            auto srcBuf = static_cast<D3D12Buffer&>(src).get();
            auto dstBuf = static_cast<D3D12Buffer&>(dst).get();

            commandList->CopyBufferRegion(dstBuf, dstOffset, srcBuf, srcOffset, size);
        }

        void copy(
            RHITexture& src,
            RHITexture& dst
        ) noexcept RHI_OVERRIDE{
            auto srcTex = static_cast<D3D12Texture&>(src).get();
            auto dstTex = static_cast<D3D12Texture&>(dst).get();

            commandList->CopyResource(srcTex, dstTex);
        }

        void copy(
            RHITexture& src,
            RHISwapchain& swapchain
        ) noexcept RHI_OVERRIDE{
            auto srcTex = static_cast<D3D12Texture&>(src).get();
        }

        void copy(
            RHIBuffer& src,
            RHITexture& dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            auto srcBuf = static_cast<D3D12Buffer&>(src).get();
            auto d3dDst = static_cast<D3D12Texture&>(dst).get();

            // Note: Requires proper footprint calculation
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource = srcBuf;
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource = d3dDst;
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = mipLevel + arraySlice;

            commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
        }

        void beginEvent(const char* name) noexcept RHI_OVERRIDE{
            commandList->BeginEvent(0, name, strlen(name));
        }

        void endEvent() noexcept RHI_OVERRIDE{
            commandList->EndEvent();
        }

        void setMarker(const char* name) noexcept RHI_OVERRIDE{
            commandList->SetMarker(0, name, strlen(name));
        }

        void* getNative() const noexcept RHI_OVERRIDE{
            return get();
        }

        ID3D12GraphicsCommandList* get() const noexcept{
            return commandList;
        }

    private:
        void beginRenderPass(
            std::span<const D3D12_RENDER_PASS_RENDER_TARGET_DESC> rtDescs,
            D3D12Texture* depthTarget,
            RHILoadAction loadAction,
            RHIStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) noexcept{
            D3D12_RENDER_PASS_DEPTH_STENCIL_DESC dsDesc;
            if(depthTarget != nullptr){
                dsDesc.cpuDescriptor = dsvHeap->getCPUHandle(depthTarget->getDSVHeapIndex());
                dsDesc.DepthBeginningAccess =
                dsDesc.StencilBeginningAccess = {.Type = convert(loadAction)};
                dsDesc.DepthEndingAccess =
                dsDesc.StencilEndingAccess = {.Type = convert(storeAction)};
                if(loadAction == RHILoadAction::Clear){
                    dsDesc.DepthBeginningAccess.Clear.ClearValue =
                    dsDesc.StencilBeginningAccess.Clear.ClearValue = {
                        .Format = convert(depthTarget->getFormat()),
                        .DepthStencil = {clearDS.depth, clearDS.stencil}
                    };
                }
            }

            commandList->BeginRenderPass(
                rtDescs.size(),
                rtDescs.data(),
                depthTarget != nullptr ? &dsDesc : nullptr,
                D3D12_RENDER_PASS_FLAG_NONE
            );
        }

        void transitionResource(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES stateBefore,
            D3D12_RESOURCE_STATES stateAfter
        ){
            if(stateBefore == stateAfter) return;

            D3D12_RESOURCE_BARRIER barrier{
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
                .Transition = {
                    .pResource = resource,
                    .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                    .StateBefore = stateBefore,
                    .StateAfter = stateAfter
                }
            };

            commandList->ResourceBarrier(1, &barrier);
        }
    };
}
