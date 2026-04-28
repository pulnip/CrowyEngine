#pragma once

#include <memory>

namespace Crowy
{
#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        class MetalDevice;
        class MetalBuffer;
        class MetalTexture;
        class MetalSampler;
        class MetalCommandList;
        class MetalFence;
        class MetalFrameScope;
        class MetalGraphicsPipelineState;
        class MetalComputePipelineState;
        class MetalSwapchain;

        using RHIDevice = MetalDevice;
        using RHIBuffer = MetalBuffer;
        using RHITexture = MetalTexture;
        using RHISampler = MetalSampler;
        using RHICommandList = MetalCommandList;
        using RHIFence = MetalFence;
        using RHIFrameScope = MetalFrameScope;
        using RHIGraphicsPipelineState = MetalGraphicsPipelineState;
        using RHIComputePipelineState = MetalComputePipelineState;
        using RHISwapchain = MetalSwapchain;
    #elif defined(USE_D3D11_BACKEND)
        class D3D11Device;
        class D3D11Buffer;
        class D3D11Texture;
        class D3D11Sampler;
        class D3D11CommandList;
        class D3D11Fence;
        class D3D11FrameScope;
        class D3D11GraphicsPipelineState;
        class D3D11ComputePipelineState;
        class D3D11Swapchain;

        using RHIDevice = D3D11Device;
        using RHIBuffer = D3D11Buffer;
        using RHITexture = D3D11Texture;
        using RHISampler = D3D11Sampler;
        using RHICommandList = D3D11CommandList;
        using RHIFence = D3D11Fence;
        using RHIFrameScope = D3D11FrameScope;
        using RHIGraphicsPipelineState = D3D11GraphicsPipelineState;
        using RHIComputePipelineState = D3D11ComputePipelineState;
        using RHISwapchain = D3D11Swapchain;
    #else
        class NullDevice;
        class NullBuffer;
        class NullTexture;
        class NullSampler;
        class NullCommandList;
        class NullFence;
        class NullFrameScope;
        class NullGraphicsPipelineState;
        class NullComputePipelineState;
        class NullSwapchain;

        using RHIDevice = NullDevice;
        using RHIBuffer = NullBuffer;
        using RHITexture = NullTexture;
        using RHISampler = NullSampler;
        using RHICommandList = NullCommandList;
        using RHIFence = NullFence;
        using RHIFrameScope = NullFrameScope;
        using RHIGraphicsPipelineState = NullGraphicsPipelineState;
        using RHIComputePipelineState = NullComputePipelineState;
        using RHISwapchain = NullSwapchain;
    #endif
#else
    class RHIDevice;
    class RHIBuffer;
    class RHITexture;
    class RHISampler;
    class RHICommandList;
    class RHIFence;
    class RHIFrameScope;
    class RHIGraphicsPipelineState;
    class RHIComputePipelineState;
    class RHISwapchain;
#endif
    class FramePacer;

    using RHIDeviceRAII = std::unique_ptr<RHIDevice>;
    using RHIBufferRAII = std::unique_ptr<RHIBuffer>;
    using RHITextureRAII = std::unique_ptr<RHITexture>;
    using RHISamplerRAII = std::unique_ptr<RHISampler>;
    using RHICommandListRAII = std::unique_ptr<RHICommandList>;
    using RHIFenceRAII = std::unique_ptr<RHIFence>;
    using RHIFrameScopeRAII = std::unique_ptr<RHIFrameScope>;
    using RHIGraphicsPipelineStateRAII = std::unique_ptr<RHIGraphicsPipelineState>;
    using RHIComputePipelineStateRAII = std::unique_ptr<RHIComputePipelineState>;
    using RHISwapchainRAII = std::unique_ptr<RHISwapchain>;
    using FramePacerRAII = std::unique_ptr<FramePacer>;
}