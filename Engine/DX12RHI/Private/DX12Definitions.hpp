#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Primitives.hpp"

namespace Crowy
{
    using Device = ID3D12Device10;
    using CommandQueue = ID3D12CommandQueue;
    using CommandAllocator = ID3D12CommandAllocator;
    using CommandList = ID3D12GraphicsCommandList7;
    using Factory = IDXGIFactory5;
    using Adapter = IDXGIAdapter1;
    using Swapchain = IDXGISwapChain3;

    using PipelineState = ID3D12PipelineState;
    using RootSignature = ID3D12RootSignature;
    using CommandSignature = ID3D12CommandSignature;

    using Buffer = ID3D12Resource;
    using Texture = ID3D12Resource;
    using DescriptorHeap = ID3D12DescriptorHeap;
    class DX12DescriptorHeapAllocator;

    using Blob = ID3DBlob;
    using Fence = ID3D12Fence;
    using QueryHeap = ID3D12QueryHeap;

    template<typename T>
    using RefPtr = Microsoft::WRL::ComPtr<T>;

    using DX12DescriptorHeapAllocatorRAII = RAII<DX12DescriptorHeapAllocator>;

    inline constexpr UINT RootParamPush = 0;
    inline constexpr UINT RootParamCBBase = 1;

    struct DX12Capabilities {
        bool gpuUploadHeap = false;
    };
}
