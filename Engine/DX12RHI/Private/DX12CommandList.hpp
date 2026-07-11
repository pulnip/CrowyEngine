#pragma once

#include "RHIDefinitions.hpp"
#include "DX12Definitions.hpp"
#include <type_traits>

namespace Crowy
{
    class DX12Buffer;
    class DX12Texture;
    class DX12GraphicsPipelineState;
    class DX12ComputePipelineState;

    class DX12CommandList{
    private:
        CommandQueue& commandQueue;
        RootSignature& rootSignature;
        CommandAllocatorRAII commandAllocators[RHI_FRAMES_IN_FLIGHT];
        const u64& frameIndex;

        CommandListRAII commandList = nullptr;
        // simulate command recording
        bool inComputePass = false;

        DescriptorHeapAllocator& cbvsrvuavHeap;
        DescriptorHeapAllocator& rtvHeap;
        DescriptorHeapAllocator& dsvHeap;
        DescriptorHeapAllocator& samplerHeap;
        // TODO. Add REAL bindless api
        DX12GraphicsPipelineState* currentGraphicsPSO = nullptr;
        DX12ComputePipelineState* currentComputePSO = nullptr;

    public:
        DX12CommandList(
            Device&,
            CommandQueue&,
            RootSignature&,
            const u64& frameIndex,
            DescriptorHeapAllocator& cbvsrvuavHeap,
            DescriptorHeapAllocator& rtvHeap,
            DescriptorHeapAllocator& dsvHeap,
            DescriptorHeapAllocator& samplerHeap
        );
        ~DX12CommandList();

        void Begin();
        void Close();

        void BeginRenderPass(const RHIRenderPassDesc&);
        void EndRenderPass();

        void SetPipelineState(DX12GraphicsPipelineState& pso);
        void SetPipelineState(DX12ComputePipelineState& pso);

        void SetVertexBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 stride,
            u32 offset
        );

        void SetIndexBuffer(
            RHIBuffer& buffer,
            RHIIndexFormat format,
            u32 offset
        );

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushGraphicsConstants(const T& t){
            setPushGraphicsConstants(&t, sizeof(T));
        }

        template<typename T>
            requires (!std::is_pointer_v<T>)
        void SetPushComputeConstants(const T& t){
            setPushComputeConstants(&t, sizeof(T));
        }

        void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        );

        void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        );

        void SetViewport(const RHIViewport& viewport);
        void SetScissorRect(const RHIScissorRect& scissor);

        void Draw(
            u32 vertexCount,
            u32 instanceCount = 1,
            u32 startVertex = 0,
            u32 startInstance = 0
        );

        void DrawIndexed(
            u32 indexCount,
            u32 instanceCount = 1,
            u32 startIndex = 0,
            i32 baseVertex = 0,
            u32 startInstance = 0
        );

        void BeginCompute() noexcept;
        void EndCompute() noexcept;

        void Dispatch(Size3D gridSize);

        void TransitionBarrier(
            DX12Texture&,
            D3D12_BARRIER_SYNC syncAfter,
            D3D12_BARRIER_ACCESS accessAfter,
            D3D12_BARRIER_LAYOUT layoutAfter
        );

        void TransitionBarrier(
            DX12Buffer&,
            D3D12_BARRIER_SYNC syncAfter,
            D3D12_BARRIER_ACCESS accessAfter
        );

        void Copy(
            RHIBuffer& src,
            RHIBuffer& dst,
            usize srcOffset,
            usize dstOffset,
            usize size
        );

        void Copy(
            RHITexture& src,
            RHITexture& dst
        );

        void Copy(
            RHITexture& src,
            RHISwapchain& dst
        );

        void Copy(
            RHIBuffer& src,
            u64 srcOffset,   // align 512 (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)
            u32 srcRowPitch, // align 256 (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
            RHITexture& dst,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        );

        void WaitUntilCompleted();

        void BeginEvent(CStr name);
        void EndEvent();
        void SetMarker(CStr name);

        void* GetNative() noexcept{ return Get(); }

        CommandList* Get() noexcept{ return commandList.Get(); }

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % RHI_FRAMES_IN_FLIGHT
            );
        }

        void setPushGraphicsConstants(
            const void* data,
            u32 size
        );
        void setPushComputeConstants(
            const void* data,
            u32 size
        );
    };
}
