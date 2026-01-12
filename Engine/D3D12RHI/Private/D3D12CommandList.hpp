#pragma once

#include <array>
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
#include "Log.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHICommandList.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    constexpr uint32_t D3D12_FRAMES_IN_FLIGHT = 3;

    class D3D12CommandList
#ifndef USE_STATIC_RHI
        : public RHICommandList
#endif
    {
    private:
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* commandQueue = nullptr;
        std::array<ComPtr<ID3D12CommandAllocator>, D3D12_FRAMES_IN_FLIGHT> commandAllocators;
        ComPtr<ID3D12GraphicsCommandList> commandList;
        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        ComPtr<ID3D12DescriptorHeap> dsvHeap;
        std::array<ComPtr<ID3D12DescriptorHeap>, D3D12_FRAMES_IN_FLIGHT> srvHeaps;

        // Internal fence for allocator synchronization
        ComPtr<ID3D12Fence> internalFence;
        std::array<uint64_t, D3D12_FRAMES_IN_FLIGHT> allocatorFenceValues = {};
        uint64_t nextFenceValue = 1;

        // Linear descriptor allocator for SRV heap
        uint32_t srvHeapOffset = 0;
        static constexpr uint32_t SRV_HEAP_SIZE = 256;

        ID3D12Resource* currentRenderTarget = nullptr;
        ID3D12Resource* currentDepthStencil = nullptr;

        RHIPrimitiveTopology currentTopology = RHIPrimitiveTopology::TriangleList;
        bool isRecording = false;
        uint32_t currentAllocatorIndex = 0;

        RHIFence* pendingFence = nullptr;
        uint64_t pendingFenceValue = 0;

        UINT rtvDescriptorSize = 0;
        UINT dsvDescriptorSize = 0;
        UINT srvDescriptorSize = 0;

    public:
        D3D12CommandList(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue
        )
            : device(device)
            , commandQueue(commandQueue)
        {
            // Create multiple command allocators for triple buffering
            for(uint32_t i = 0; i < D3D12_FRAMES_IN_FLIGHT; ++i){
                if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[i])))){
                    throw std::runtime_error("Failed to create command allocator");
                }
            }

            if(FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList)))){
                throw std::runtime_error("Failed to create command list");
            }

            commandList->Close();

            // Create internal fence for allocator synchronization
            if(FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&internalFence)))){
                throw std::runtime_error("Failed to create internal fence");
            }

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
            for(uint32_t i = 0; i < D3D12_FRAMES_IN_FLIGHT; ++i){
                if(FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeaps[i])))){
                    throw std::runtime_error("Failed to create SRV descriptor heap");
                }
            }

            rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        ~D3D12CommandList(){
        }

        void begin() RHI_OVERRIDE{
            if(isRecording) return;

            // Wait for this allocator's previous work to complete
            uint64_t waitValue = allocatorFenceValues[currentAllocatorIndex];
            if(internalFence->GetCompletedValue() < waitValue){
                HANDLE event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
                if(event){
                    internalFence->SetEventOnCompletion(waitValue, event);
                    WaitForSingleObject(event, INFINITE);
                    CloseHandle(event);
                }
            }

            auto& allocator = commandAllocators[currentAllocatorIndex];
            allocator->Reset();
            commandList->Reset(allocator.Get(), nullptr);
            isRecording = true;

            // Reset linear descriptor allocator
            srvHeapOffset = 0;

            ID3D12DescriptorHeap* heaps[] = { srvHeaps[currentAllocatorIndex].Get() };
            commandList->SetDescriptorHeaps(1, heaps);
        }

        void close() RHI_OVERRIDE{
            if(!isRecording) return;
            commandList->Close();
            isRecording = false;
        }

        // Called by Device::submit() after ExecuteCommandLists
        void signalAllocatorFence(){
            allocatorFenceValues[currentAllocatorIndex] = nextFenceValue;
            commandQueue->Signal(internalFence.Get(), nextFenceValue);
            ++nextFenceValue;

            // Cycle to next allocator
            currentAllocatorIndex = (currentAllocatorIndex + 1) % D3D12_FRAMES_IN_FLIGHT;
        }

        void reset() RHI_OVERRIDE{
            currentRenderTarget = nullptr;
            currentDepthStencil = nullptr;
            pendingFence = nullptr;
            pendingFenceValue = 0;
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

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};

            if(d3dRT){
                // Transition to render target
                transitionResource(d3dRT, static_cast<D3D12Texture*>(renderTarget)->getState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
                static_cast<D3D12Texture*>(renderTarget)->getState() = D3D12_RESOURCE_STATE_RENDER_TARGET;

                rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
                device->CreateRenderTargetView(d3dRT, nullptr, rtvHandle);

                // Note: Should use proper descriptor management in production
                if(loadAction == RHILoadStoreAction::Clear){
                    float clearColorArray[4] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};
                    commandList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
                }
            }

            if(d3dDS){
                transitionResource(d3dDS, static_cast<D3D12Texture*>(depthStencil)->getState(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
                static_cast<D3D12Texture*>(depthStencil)->getState() = D3D12_RESOURCE_STATE_DEPTH_WRITE;

                dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
                device->CreateDepthStencilView(d3dDS, nullptr, dsvHandle);

                if(loadAction == RHILoadStoreAction::Clear){
                    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
                    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, clearDS.depth, clearDS.stencil, 0, nullptr);
                }
            }

            commandList->OMSetRenderTargets(
                d3dRT ? 1 : 0,
                d3dRT ? &rtvHandle : nullptr,
                FALSE,
                d3dDS ? &dsvHandle : nullptr
            );

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

            // Create RTV for back buffer
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);

            if(loadAction == RHILoadStoreAction::Clear){
                float clearColorArray[4] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};
                commandList->ClearRenderTargetView(rtvHandle, clearColorArray, 0, nullptr);
            }

            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
            if(depthStencil){
                auto d3dDS = static_cast<D3D12Texture*>(depthStencil)->get();
                transitionResource(d3dDS, static_cast<D3D12Texture*>(depthStencil)->getState(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
                static_cast<D3D12Texture*>(depthStencil)->getState() = D3D12_RESOURCE_STATE_DEPTH_WRITE;

                // Create DSV for depth buffer
                dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
                device->CreateDepthStencilView(d3dDS, nullptr, dsvHandle);

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
            RHIShaderStage stage,
            uint32_t slot,
            RHIBuffer* buffer,
            uint32_t offset = 0
        ) RHI_OVERRIDE{
            if(!buffer || !isRecording) return;

            auto d3dBuffer = static_cast<D3D12Buffer*>(buffer)->get();
            if(!d3dBuffer) return;

            D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = d3dBuffer->GetGPUVirtualAddress();

            // Root Param 0: CBV at b1 for Vertex Shader
            // Root Param 1: CBV at b2 for Pixel Shader
            // Root Param 2: SRV descriptor table at t0 for Pixel Shader
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

            auto d3dTexture = static_cast<D3D12Texture*>(texture);
            auto texResource = d3dTexture->get();
            if(!texResource) return;

            auto texDesc = texResource->GetDesc();

            // Transition to shader resource state
            if(d3dTexture->getState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE){
                transitionResource(
                    texResource,
                    d3dTexture->getState(),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                );
                d3dTexture->getState() = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            }

            // Use linear allocator pattern - each setTexture call gets a new descriptor slot
            if(srvHeapOffset >= SRV_HEAP_SIZE){
                // Heap exhausted, wrap around (should not happen in normal usage)
                srvHeapOffset = 0;
            }

            auto& currentSrvHeap = srvHeaps[currentAllocatorIndex];
            D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = currentSrvHeap->GetCPUDescriptorHandleForHeapStart();
            srvCpuHandle.ptr += srvHeapOffset * srvDescriptorSize;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = texDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            device->CreateShaderResourceView(texResource, &srvDesc, srvCpuHandle);

            // Set descriptor table
            D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle = currentSrvHeap->GetGPUDescriptorHandleForHeapStart();
            srvGpuHandle.ptr += srvHeapOffset * srvDescriptorSize;
            commandList->SetGraphicsRootDescriptorTable(2, srvGpuHandle);

            // Advance to next slot
            ++srvHeapOffset;
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
            pendingFence = fence;
            pendingFenceValue = value;
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

        RHIFence* getPendingFence() const{
            return pendingFence;
        }

        uint64_t getPendingFenceValue() const{
            return pendingFenceValue;
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
