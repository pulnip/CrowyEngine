#pragma once

#include <cstddef>
#include <memory>
#include <d3d12.h>
#include <wrl/client.h>
#include "D3D12Buffer.hpp"
#include "D3D12Fence.hpp"
#include "D3D12PipelineState.hpp"
#include "D3D12Swapchain.hpp"
#include "D3D12Texture.hpp"
#include "D3D12Util.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12CommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
    private:
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12DescriptorHeap> dsvHeap;
        ComPtr<ID3D12DescriptorHeap> srvHeap;

        ID3D12Resource* currentRenderTarget = nullptr;
        ID3D12Resource* currentDepthStencil = nullptr;

        RHIPrimitiveTopology currentTopology = RHIPrimitiveTopology::TriangleList;
        bool isRecording = false;

        UINT rtvDescriptorSize = 0;
        UINT dsvDescriptorSize = 0;
        UINT srvDescriptorSize = 0;

    public:
        D3D12CommandList(
            ID3D12Device* device
        ){
            if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)))){
                throw std::runtime_error("Failed to create command allocator");
            }

            if(FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)))){
                throw std::runtime_error("Failed to create command list");
            }

            commandList->Close();

            // Create descriptor heaps
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = 16;
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if(FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))){
                throw std::runtime_error("Failed to create RTV descriptor heap");
            }

            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.NumDescriptors = 16;
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if(FAILED(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)))){
                throw std::runtime_error("Failed to create DSV descriptor heap");
            }

            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = 256;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if(FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)))){
                throw std::runtime_error("Failed to create SRV descriptor heap");
            }

            rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        ~D3D12CommandList(){
        }

        void begin() RHI_OVERRIDE{
            if(isRecording) return;

            commandAllocator->Reset();
            commandList->Reset(commandAllocator.Get(), nullptr);
            isRecording = true;

            ID3D12DescriptorHeap* heaps[] = { srvHeap.Get() };
            commandList->SetDescriptorHeaps(1, heaps);
        }

        void close() RHI_OVERRIDE{
            if(!isRecording) return;
            commandList->Close();
            isRecording = false;
        }

        void reset() RHI_OVERRIDE{
            currentRenderTarget = nullptr;
            currentDepthStencil = nullptr;
            isRecording = false;
        }

        void beginRenderPass(
            RHITexture* renderTarget,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) RHI_OVERRIDE{
            if(!isRecording) return;

            auto d3dRT = renderTarget ? static_cast<D3D12Texture*>(renderTarget)->get() : nullptr;
            auto d3dDS = depthStencil ? static_cast<D3D12Texture*>(depthStencil)->get() : nullptr;

            if(d3dRT){
                // Transition to render target
                transitionResource(d3dRT, static_cast<D3D12Texture*>(renderTarget)->getState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
                static_cast<D3D12Texture*>(renderTarget)->getState() = D3D12_RESOURCE_STATE_RENDER_TARGET;

                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
                // Note: Should use proper descriptor management in production
                if(loadAction == RHILoadStoreAction::Clear){
                    float clearColorArray[4] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};
                    commandList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
                }
            }

            if(d3dDS){
                transitionResource(d3dDS, static_cast<D3D12Texture*>(depthStencil)->getState(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
                static_cast<D3D12Texture*>(depthStencil)->getState() = D3D12_RESOURCE_STATE_DEPTH_WRITE;

                if(loadAction == RHILoadStoreAction::Clear){
                    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
                    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, clearDS.depth, clearDS.stencil, 0, nullptr);
                }
            }

            currentRenderTarget = d3dRT;
            currentDepthStencil = d3dDS;
        }

        void beginRenderPass(
            RHISwapchain* swapchain,
            RHITexture* depthStencil,
            RHILoadStoreAction loadAction,
            RHILoadStoreAction storeAction,
            const RHIClearColor& clearColor,
            const RHIClearDepthStencil& clearDS
        ) RHI_OVERRIDE{
            if(!swapchain || !isRecording) return;

            auto d3dSwapchain = static_cast<D3D12Swapchain*>(swapchain);
            auto backBuffer = d3dSwapchain->getCurrentBackBuffer();

            // Transition swapchain to render target
            transitionResource(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            if(loadAction == RHILoadStoreAction::Clear){
                float clearColorArray[4] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};
                commandList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
            }

            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
            if(depthStencil){
                auto d3dDS = static_cast<D3D12Texture*>(depthStencil)->get();
                transitionResource(d3dDS, static_cast<D3D12Texture*>(depthStencil)->getState(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
                static_cast<D3D12Texture*>(depthStencil)->getState() = D3D12_RESOURCE_STATE_DEPTH_WRITE;

                dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
                if(loadAction == RHILoadStoreAction::Clear){
                    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, clearDS.depth, clearDS.stencil, 0, nullptr);
                }
                currentDepthStencil = d3dDS;
            }

            commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, depthStencil ? &dsvHandle : nullptr);
            currentRenderTarget = backBuffer;
        }

        void endRenderPass() RHI_OVERRIDE{
            if(!isRecording) return;

            // Transition swapchain back to present if needed
            if(currentRenderTarget){
                transitionResource(currentRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            }
        }

        void setPipelineState(RHIPipelineState* pso) RHI_OVERRIDE{
            if(!pso || !isRecording) return;

            auto d3dPSO = static_cast<D3D12PipelineState*>(pso);
            commandList->SetPipelineState(d3dPSO->get());
            commandList->SetGraphicsRootSignature(d3dPSO->getRootSignature());
            currentTopology = d3dPSO->getTopology();
            commandList->IASetPrimitiveTopology(convertTopology(currentTopology));
        }

        void setVertexBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            uint32_t stride,
            uint32_t offset = 0
        ) RHI_OVERRIDE{
            if(!buffer || !isRecording) return;

            auto d3dBuffer = static_cast<D3D12Buffer*>(buffer)->get();
            D3D12_VERTEX_BUFFER_VIEW vbView = {};
            vbView.BufferLocation = d3dBuffer->GetGPUVirtualAddress() + offset;
            vbView.SizeInBytes = static_cast<UINT>(d3dBuffer->GetDesc().Width - offset);
            vbView.StrideInBytes = stride;

            commandList->IASetVertexBuffers(slot, 1, &vbView);
        }

        void setIndexBuffer(
            RHIBuffer* buffer,
            RHIIndexFormat format,
            uint32_t offset = 0
        ) RHI_OVERRIDE{
            if(!buffer || !isRecording) return;

            auto d3dBuffer = static_cast<D3D12Buffer*>(buffer)->get();
            D3D12_INDEX_BUFFER_VIEW ibView = {};
            ibView.BufferLocation = d3dBuffer->GetGPUVirtualAddress() + offset;
            ibView.SizeInBytes = static_cast<UINT>(d3dBuffer->GetDesc().Width - offset);
            ibView.Format = (format == RHIIndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

            commandList->IASetIndexBuffer(&ibView);
        }

        void setConstantBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            RHIShaderStage stage
        ) RHI_OVERRIDE{
            if(!buffer || !isRecording) return;

            auto d3dBuffer = static_cast<D3D12Buffer*>(buffer)->get();
            D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = d3dBuffer->GetGPUVirtualAddress();

            // Use root descriptor for CBV (slot + 1 to skip descriptor table at root param 0)
            if(stage == RHIShaderStage::VertexShader){
                commandList->SetGraphicsRootConstantBufferView(0, cbvAddress);
            }
            else if(stage == RHIShaderStage::FragmentShader){
                commandList->SetGraphicsRootConstantBufferView(1, cbvAddress);
            }
        }

        void setTexture(
            uint32_t slot,
            RHITexture* texture,
            RHIShaderStage stage
        ) RHI_OVERRIDE{
            if(!texture || !isRecording) return;

            // Note: Requires proper SRV creation in descriptor heap
            // For now, set descriptor table (simplified)
            D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
            commandList->SetGraphicsRootDescriptorTable(2, srvHandle);
        }

        void setBuffer(
            uint32_t slot,
            RHIBuffer* buffer,
            RHIShaderStage stage
        ) RHI_OVERRIDE{
            // Similar to setTexture, requires descriptor heap management
        }

        void setViewport(const RHIViewport& viewport) RHI_OVERRIDE{
            if(!isRecording) return;

            D3D12_VIEWPORT vp = {};
            vp.TopLeftX = viewport.x;
            vp.TopLeftY = viewport.y;
            vp.Width = viewport.width;
            vp.Height = viewport.height;
            vp.MinDepth = viewport.minDepth;
            vp.MaxDepth = viewport.maxDepth;

            commandList->RSSetViewports(1, &vp);
        }

        void setScissorRect(const RHIScissorRect& scissor) RHI_OVERRIDE{
            if(!isRecording) return;

            D3D12_RECT rect = {};
            rect.left = scissor.left;
            rect.top = scissor.top;
            rect.right = scissor.right;
            rect.bottom = scissor.bottom;

            commandList->RSSetScissorRects(1, &rect);
        }

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t startVertex = 0,
            uint32_t startInstance = 0
        ) RHI_OVERRIDE{
            if(!isRecording) return;

            commandList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
        }

        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t startIndex = 0,
            int32_t baseVertex = 0,
            uint32_t startInstance = 0
        ) RHI_OVERRIDE{
            if(!isRecording) return;

            commandList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
        }

        void dispatch(
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY,
            uint32_t threadGroupCountZ
        ) RHI_OVERRIDE{
            if(!isRecording) return;

            commandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
        }

        void transitionBarrier(
            RHITexture* texture,
            RHIResourceState before,
            RHIResourceState after
        ) RHI_OVERRIDE{
            if(!texture || !isRecording) return;

            auto d3dTexture = static_cast<D3D12Texture*>(texture)->get();
            transitionResource(d3dTexture, convertResourceState(before), convertResourceState(after));
        }

        void transitionBarrier(
            RHIBuffer* buffer,
            RHIResourceState before,
            RHIResourceState after
        ) RHI_OVERRIDE{
            if(!buffer || !isRecording) return;

            auto d3dBuffer = static_cast<D3D12Buffer*>(buffer)->get();
            transitionResource(d3dBuffer, convertResourceState(before), convertResourceState(after));
        }

        void uavBarrier(RHITexture* texture) RHI_OVERRIDE{
            if(!texture || !isRecording) return;

            auto d3dTexture = static_cast<D3D12Texture*>(texture)->get();
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = d3dTexture;

            commandList->ResourceBarrier(1, &barrier);
        }

        void uavBarrier(RHIBuffer* buffer) RHI_OVERRIDE{
            if(!buffer || !isRecording) return;

            auto d3dBuffer = static_cast<D3D12Buffer*>(buffer)->get();
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = d3dBuffer;

            commandList->ResourceBarrier(1, &barrier);
        }

        void signalFence(RHIFence* fence, uint64_t value) RHI_OVERRIDE{
            // Note: Fence signaling is done via command queue, not command list
            // This is handled in the device submit
        }

        void waitFence(RHIFence* fence, uint64_t value) RHI_OVERRIDE{
            // Note: Fence waiting is done via command queue, not command list
        }

        void copyBuffer(
            RHIBuffer* src,
            RHIBuffer* dst,
            size_t srcOffset,
            size_t dstOffset,
            size_t size
        ) RHI_OVERRIDE{
            if(!src || !dst || !isRecording) return;

            auto d3dSrc = static_cast<D3D12Buffer*>(src)->get();
            auto d3dDst = static_cast<D3D12Buffer*>(dst)->get();

            commandList->CopyBufferRegion(d3dDst, dstOffset, d3dSrc, srcOffset, size);
        }

        void copyTexture(
            RHITexture* src,
            RHITexture* dst
        ) RHI_OVERRIDE{
            if(!src || !dst || !isRecording) return;

            auto d3dSrc = static_cast<D3D12Texture*>(src)->get();
            auto d3dDst = static_cast<D3D12Texture*>(dst)->get();

            commandList->CopyResource(d3dDst, d3dSrc);
        }

        void copyBufferToTexture(
            RHIBuffer* src,
            RHITexture* dst,
            uint32_t mipLevel = 0,
            uint32_t arraySlice = 0
        ) RHI_OVERRIDE{
            if(!src || !dst || !isRecording) return;

            auto d3dSrc = static_cast<D3D12Buffer*>(src)->get();
            auto d3dDst = static_cast<D3D12Texture*>(dst)->get();

            // Note: Requires proper footprint calculation
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource = d3dSrc;
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint = footprint;

            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource = d3dDst;
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = mipLevel + arraySlice;

            commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
        }

        void beginEvent(const char* name) RHI_OVERRIDE{
            // PIX event markers (requires d3d12.h with PIX support)
            // commandList->BeginEvent(0, name, strlen(name));
        }

        void endEvent() RHI_OVERRIDE{
            // commandList->EndEvent();
        }

        void setMarker(const char* name) RHI_OVERRIDE{
            // commandList->SetMarker(0, name, strlen(name));
        }

        ID3D12GraphicsCommandList* get() const{
            return commandList.Get();
        }

    private:
        void transitionResource(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES stateBefore,
            D3D12_RESOURCE_STATES stateAfter
        ){
            if(stateBefore == stateAfter) return;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = resource;
            barrier.Transition.StateBefore = stateBefore;
            barrier.Transition.StateAfter = stateAfter;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            commandList->ResourceBarrier(1, &barrier);
        }
    };
}
