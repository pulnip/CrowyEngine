#pragma once

#include <memory>
#include "generic_handle.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        class MetalDevice;
        class MetalBuffer;
        class MetalTexture;
        class MetalShader;
        class MetalCommandList;
        class MetalFence;
        class MetalPipelineState;
        class MetalSwapchain;

        using RHIDevice = MetalDevice;
        using RHIBuffer = MetalBuffer;
        using RHITexture = MetalTexture;
        using RHIShader = MetalShader;
        using RHICommandList = MetalCommandList;
        using RHIFence = MetalFence;
        using RHIPipelineState = MetalPipelineState;
        using RHISwapchain = MetalSwapchain;
    #else
        class NullDevice;
        class NullBuffer;
        class NullTexture;
        class NullShader;
        class NullCommandList;
        class NullFence;
        class NullPipelineState;
        class NullSwapchain;

        using RHIDevice = NullDevice;
        using RHIBuffer = NullBuffer;
        using RHITexture = NullTexture;
        using RHIShader = NullShader;
        using RHICommandList = NullCommandList;
        using RHIFence = NullFence;
        using RHIPipelineState = NullPipelineState;
        using RHISwapchain = NullSwapchain;
    #endif
#else
    class RHIDevice;
    class RHIBuffer;
    class RHITexture;
    class RHIShader;
    class RHICommandList;
    class RHIFence;
    class RHIPipelineState;
    class RHISwapchain;
#endif
    class FramePacer;

    using RHIDevicePtr        = std::unique_ptr<RHIDevice>;
    using RHIBufferPtr        = std::unique_ptr<RHIBuffer>;
    using RHITexturePtr       = std::unique_ptr<RHITexture>;
    using RHIShaderPtr        = std::unique_ptr<RHIShader>;
    using RHICommandListPtr   = std::unique_ptr<RHICommandList>;
    using RHIFencePtr         = std::unique_ptr<RHIFence>;
    using RHIPipelineStatePtr = std::unique_ptr<RHIPipelineState>;
    using RHISwapchainPtr     = std::unique_ptr<RHISwapchain>;
    using FramePacerPtr       = std::unique_ptr<FramePacer>;
}