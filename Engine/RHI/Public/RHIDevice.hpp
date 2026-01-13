#pragma once

#include <memory>
#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalDevice.hpp"
    #else
        #include "NullDevice.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIDeviceType = requires(T device,
        RHIBufferCreateDesc bufDesc,
        RHITextureCreateDesc texDesc,
        RHIShaderCreateDesc shaderDesc,
        RHIGraphicsPipelineStateDesc gpsDesc,
        RHIComputePipelineStateDesc cpsDesc
    ){
        { device.createBuffer(bufDesc) } -> std::same_as<RHIBufferPtr>;
        { device.createTexture(texDesc) } -> std::same_as<RHITexturePtr>;
        { device.createShader(shaderDesc) } -> std::same_as<RHIShaderPtr>;

        { device.createGraphicsPipelineState(gpsDesc) } -> std::same_as<RHIPipelineStatePtr>;
        { device.createComputePipelineState(cpsDesc) } -> std::same_as<RHIPipelineStatePtr>;

        { device.createFence(uint64_t(0)) } -> std::same_as<RHIFencePtr>;

        { device.getCapabilities() } -> std::same_as<RHICapabilities>;
    };
    static_assert(RHIDeviceType<RHIDevice>);
#else
    class RHIDevice{
    public:
        CROWY_DECLARE_INTERFACE(RHIDevice)

        virtual RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) = 0;
        virtual RHITexturePtr createTexture(const RHITextureCreateDesc&) = 0;
        virtual RHIShaderPtr  createShader (const RHIShaderCreateDesc& ) = 0;

        virtual RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc&
        ) = 0;
        virtual RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc&
        ) = 0;

        virtual RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc&
        ) = 0;

        virtual RHICommandListPtr createCommandList() = 0;

        virtual RHIFencePtr createFence(uint64_t initialValue = 0) = 0;

        FramePacerPtr createFramePacer();

        virtual RHICapabilities getCapabilities() const = 0;

        virtual void submit(RHICommandList*, RHISwapchain* presentTarget = nullptr) = 0;

        virtual void* getNative() = 0;
    };
#endif

    // each platform should implement this function
    RHIDevicePtr createDevice();
}