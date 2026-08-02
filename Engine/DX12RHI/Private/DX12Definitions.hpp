#pragma once

#include <d3d12.h>
#include <d3d12shader.h>
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

    // Pipeline state types
    using PipelineState = ID3D12PipelineState;
    using RootSignature = ID3D12RootSignature;
    using CommandSignature = ID3D12CommandSignature;

    // Resource types
    using Buffer = ID3D12Resource;
    using Texture = ID3D12Resource;
    using DescriptorHeap = ID3D12DescriptorHeap;
    class DescriptorHeapAllocator;

    using Blob = ID3DBlob;
    using Fence = ID3D12Fence;
    using ShaderReflection = ID3D12ShaderReflection;

    // RAII wrappers for COM interfaces
    template<typename T>
    using COMRAII = Microsoft::WRL::ComPtr<T>;

    using DeviceRAII = COMRAII<Device>;
    using CommandQueueRAII = COMRAII<CommandQueue>;
    using CommandAllocatorRAII = COMRAII<CommandAllocator>;
    using CommandListRAII = COMRAII<CommandList>;
    using FactoryRAII = COMRAII<Factory>;
    using AdapterRAII = COMRAII<Adapter>;
    using SwapchainRAII = COMRAII<Swapchain>;

    using PipelineStateRAII = COMRAII<PipelineState>;
    using RootSignatureRAII = COMRAII<RootSignature>;
    using CommandSignatureRAII = COMRAII<CommandSignature>;

    using BufferRAII = COMRAII<Buffer>;
    using TextureRAII = COMRAII<Texture>;
    using DescriptorHeapRAII = COMRAII<DescriptorHeap>;
    using DescriptorHeapAllocatorRAII = RAII<DescriptorHeapAllocator>;
    using ResourceState = D3D12_RESOURCE_STATES;

	using BlobRAII = COMRAII<Blob>;
    using FenceRAII = COMRAII<Fence>;
    using ShaderReflectionRAII = COMRAII<ShaderReflection>;

    inline constexpr UINT RootParamPush = 0;
    inline constexpr UINT RootParamCBBase = 1;

    // TODO. merge to RHICapabilities
    struct DX12Capabilities{
        bool gpuUploadHeap = false;
        // both required by SV_StartInstanceLocation
        bool shaderModel6_8 = false;
        bool extendedCommandInfo = false;
    };
}
