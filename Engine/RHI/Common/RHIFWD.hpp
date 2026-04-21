#pragma once

#include <memory>

namespace Crowy
{
#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        class MetalDevice;
        class MetalBuffer;
        class MetalTexture;
        class MetalShader;
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
        using RHIShader = MetalShader;
        using RHISampler = MetalSampler;
        using RHICommandList = MetalCommandList;
        using RHIFence = MetalFence;
        using RHIFrameScope = MetalFrameScope;
        using RHIGraphicsPipelineState = MetalGraphicsPipelineState;
        using RHIComputePipelineState = MetalComputePipelineState;
        using RHISwapchain = MetalSwapchain;
    #else
        class NullDevice;
        class NullBuffer;
        class NullTexture;
        class NullShader;
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
        using RHIShader = NullShader;
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
    class RHIShader;
    class RHISampler;
    class RHICommandList;
    class RHIFence;
    class RHIFrameScope;
    class RHIGraphicsPipelineState;
    class RHIComputePipelineState;
    class RHISwapchain;
#endif
    class FramePacer;

    using RHIDevicePtr = std::unique_ptr<RHIDevice>;
    using RHIBufferPtr = std::unique_ptr<RHIBuffer>;
    using RHITexturePtr = std::unique_ptr<RHITexture>;
    using RHIShaderPtr = std::unique_ptr<RHIShader>;
    using RHISamplerPtr = std::unique_ptr<RHISampler>;
    using RHICommandListPtr = std::unique_ptr<RHICommandList>;
    using RHIFencePtr = std::unique_ptr<RHIFence>;
    using RHIFrameScopePtr = std::unique_ptr<RHIFrameScope>;
    using RHIGraphicsPipelineStatePtr = std::unique_ptr<RHIGraphicsPipelineState>;
    using RHIComputePipelineStatePtr = std::unique_ptr<RHIComputePipelineState>;
    using RHISwapchainPtr = std::unique_ptr<RHISwapchain>;
    using FramePacerPtr = std::unique_ptr<FramePacer>;
}