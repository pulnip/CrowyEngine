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
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIDevice)

        virtual RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) noexcept = 0;
        virtual RHITexturePtr createTexture(const RHITextureCreateDesc&) noexcept = 0;
        virtual RHIShaderPtr  createShader (const RHIShaderCreateDesc& ) noexcept = 0;
        virtual RHISamplerPtr createSampler(const RHISamplerState&) noexcept = 0;

        virtual RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc&
        ) noexcept = 0;
        virtual RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc&
        ) noexcept = 0;

        virtual RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc&
        ) noexcept = 0;

        virtual RHICommandListPtr createCommandList() noexcept = 0;

        virtual RHIFencePtr createFence(uint64_t initialValue = 0) noexcept = 0;

        FramePacerPtr createFramePacer() noexcept;

        virtual RHICapabilities getCapabilities() const noexcept = 0;

        virtual void submit(RHICommandList&, RHISwapchain&) noexcept = 0;

        virtual void* getNative() noexcept = 0;
    };
#endif

    // each platform should implement this function
    RHIDevicePtr createDevice() noexcept;
}