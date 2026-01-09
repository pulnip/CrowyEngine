#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <d3d12.h>
#include <wrl/client.h>
#include "D3D12Util.hpp"
#include "Log.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12Texture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        ID3D12Device* device;
        ID3D12CommandQueue* commandQueue;
        ComPtr<ID3D12Resource> texture;
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        D3D12_RESOURCE_STATES currentState;

    public:
        D3D12Texture(
            ID3D12Device* device,
            ID3D12CommandQueue* commandQueue,
            const RHITextureCreateDesc& desc
        )
            : device(device), commandQueue(commandQueue)
            , width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(convertResourceState(desc.initialState))
        {
            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = (desc.depth > 1) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = desc.width;
            resourceDesc.Height = desc.height;
            resourceDesc.DepthOrArraySize = (desc.depth > 1) ? desc.depth : desc.arraySize;
            resourceDesc.MipLevels = desc.mipLevels;
            resourceDesc.Format = convertTextureFormat(desc.format);
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            if(hasFlag(desc.usage, RHITextureUsage::RenderTarget)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            }
            if(hasFlag(desc.usage, RHITextureUsage::DepthStencil)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            }
            if(hasFlag(desc.usage, RHITextureUsage::UnorderedAccess)){
                resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_CLEAR_VALUE clearValue = {};
            D3D12_CLEAR_VALUE* pClearValue = nullptr;

            if(hasFlag(desc.usage, RHITextureUsage::RenderTarget)){
                clearValue.Format = resourceDesc.Format;
                clearValue.Color[0] = desc.clearColor.r;
                clearValue.Color[1] = desc.clearColor.g;
                clearValue.Color[2] = desc.clearColor.b;
                clearValue.Color[3] = desc.clearColor.a;
                pClearValue = &clearValue;
            }
            else if(hasFlag(desc.usage, RHITextureUsage::DepthStencil)){
                clearValue.Format = resourceDesc.Format;
                clearValue.DepthStencil.Depth = desc.clearDepthStencil.depth;
                clearValue.DepthStencil.Stencil = desc.clearDepthStencil.stencil;
                pClearValue = &clearValue;
            }

            // D3D12 requires textures in DEFAULT heap to start in COMMON state
            // We'll transition to COPY_DEST later if needed for initial data upload
            D3D12_RESOURCE_STATES creationState = D3D12_RESOURCE_STATE_COMMON;

            if(FAILED(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                creationState,
                pClearValue,
                IID_PPV_ARGS(&texture)
            ))){
                throw std::runtime_error("Failed to create texture");
            }

            currentState = D3D12_RESOURCE_STATE_COMMON;

            if(desc.initialData){
                uploadData(desc.initialData, 0, 0);
            }
        }

        ~D3D12Texture(){
        }

        void uploadData(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) RHI_OVERRIDE{
            if(!data || !device || !commandQueue){
                LOG_ERROR(LOG_RHI, "uploadData: invalid args - data={}, device={}, commandQueue={}",
                    (void*)data, (void*)device, (void*)commandQueue);
                return;
            }

            auto bytesPerPixel = getBytesPerPixel(format);
            if(bytesPerPixel == 0){
                LOG_ERROR(LOG_RHI, "uploadData: bytesPerPixel is 0 for format {}", static_cast<int>(format));
                return;
            }

            UINT64 srcRowPitch = width * bytesPerPixel;
            UINT64 rowPitch = (srcRowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                & ~(static_cast<UINT64>(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) - 1);
            UINT64 uploadSize = rowPitch * height;

            auto srcBytes = static_cast<const uint8_t*>(data);

            // Use GetCopyableFootprints for correct layout calculation
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 totalBytes = 0;

            auto texDesc = texture->GetDesc();
            device->GetCopyableFootprints(
                &texDesc,
                mipLevel,    // FirstSubresource
                1,           // NumSubresources
                0,           // BaseOffset
                &footprint,
                &numRows,
                &rowSizeInBytes,
                &totalBytes
            );


            D3D12_HEAP_PROPERTIES uploadHeapProps = {};
            uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC uploadBufferDesc = {};
            uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            uploadBufferDesc.Width = totalBytes;
            uploadBufferDesc.Height = 1;
            uploadBufferDesc.DepthOrArraySize = 1;
            uploadBufferDesc.MipLevels = 1;
            uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            uploadBufferDesc.SampleDesc.Count = 1;
            uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            ComPtr<ID3D12Resource> uploadBuffer;
            if(FAILED(device->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &uploadBufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&uploadBuffer)
            ))){
                return;
            }

            void* mapped = nullptr;
            if(FAILED(uploadBuffer->Map(0, nullptr, &mapped))){
                return;
            }

            auto srcData = static_cast<const uint8_t*>(data);
            auto dstData = static_cast<uint8_t*>(mapped);
            // Use footprint's rowPitch for destination alignment
            UINT64 dstRowPitch = footprint.Footprint.RowPitch;
            for(UINT row = 0; row < numRows; ++row){
                memcpy(dstData + row * dstRowPitch, srcData + row * srcRowPitch, rowSizeInBytes);
            }

            uploadBuffer->Unmap(0, nullptr);

            ComPtr<ID3D12CommandAllocator> cmdAllocator;
            if(FAILED(device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&cmdAllocator)
            ))){
                return;
            }

            ComPtr<ID3D12GraphicsCommandList> cmdList;
            if(FAILED(device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                cmdAllocator.Get(),
                nullptr,
                IID_PPV_ARGS(&cmdList)
            ))){
                return;
            }

            // Transition to COPY_DEST if not already in that state
            if(currentState != D3D12_RESOURCE_STATE_COPY_DEST){
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = texture.Get();
                barrier.Transition.StateBefore = currentState;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                cmdList->ResourceBarrier(1, &barrier);
            }

            D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
            srcLoc.pResource = uploadBuffer.Get();
            srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLoc.PlacedFootprint = footprint;  // Use the footprint from GetCopyableFootprints

            D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
            dstLoc.pResource = texture.Get();
            dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLoc.SubresourceIndex = mipLevel;  // For single array texture

            cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

            // After copy, transition to shader resource state for use in rendering
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = texture.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmdList->ResourceBarrier(1, &barrier);
            currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            cmdList->Close();

            ID3D12CommandList* cmdLists[] = { cmdList.Get() };
            commandQueue->ExecuteCommandLists(1, cmdLists);

            ComPtr<ID3D12Fence> fence;
            if(FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))){
                return;
            }

            HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if(!fenceEvent) return;

            commandQueue->Signal(fence.Get(), 1);
            if(fence->GetCompletedValue() < 1){
                fence->SetEventOnCompletion(1, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
            CloseHandle(fenceEvent);
        }

        ID3D12Resource* get() const{ return texture.Get(); }

        D3D12_RESOURCE_STATES& getState(){ return currentState; }
    };
}
