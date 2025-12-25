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

        using RHIDevice = MetalDevice;
        using RHIBuffer = MetalBuffer;
        using RHITexture = MetalTexture;
    #else
        class NullDevice;
        class NullBuffer;
        class NullTexture;

        using RHIDevice = NullDevice;
        using RHIBuffer = NullBuffer;
        using RHITexture = NullTexture;
    #endif
#else
    class RHIDevice;
    class RHIBuffer;
    class RHITexture;
#endif
    class RHICommandList;
    class RHIResource;
    class RHIShader;
    class RHIPipelineState;
    class RHISwapchain;
    class RHIFence;

    using RHIDevicePtr        = std::unique_ptr<RHIDevice>;
    using RHIBufferPtr        = std::unique_ptr<RHIBuffer>;
    using RHITexturePtr       = std::unique_ptr<RHITexture>;
    using RHIShaderPtr        = std::unique_ptr<RHIShader>;
    using RHIPipelineStatePtr = std::unique_ptr<RHIPipelineState>;
    using RHISwapchainPtr     = std::unique_ptr<RHISwapchain>;
    using RHIFencePtr         = std::unique_ptr<RHIFence>;
}