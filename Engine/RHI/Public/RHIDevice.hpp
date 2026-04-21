#pragma once

#include <memory>
#include <string>
#include "semantics.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIFence.hpp"
#include "RHIFrameScope.hpp"
#include "RHIPipelineState.hpp"
#include "RHISampler.hpp"
#include "RHIShader.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"

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

        virtual RHIFrameScopePtr createFrameScope() noexcept = 0;

        virtual RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc&,
            const std::string& name = ""
        ) noexcept = 0;
        virtual RHITexturePtr createTexture(
            const RHITextureCreateDesc&,
            const std::string& name = ""
        ) noexcept = 0;
        virtual RHIShaderPtr createShader(
            const RHIShaderCreateDesc&
        ) = 0;
        virtual RHISamplerPtr createSampler(
            const RHISamplerState&
        ) noexcept = 0;

        virtual RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc&,
            const std::string& name = ""
        ) noexcept = 0;
        virtual RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc&,
            const std::string& name = ""
        ) noexcept = 0;

        virtual RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc&
        ) noexcept = 0;

        virtual RHICommandListPtr createCommandList() noexcept = 0;

        virtual RHIFencePtr createFence(uint64_t initialValue = 0) noexcept = 0;

        FramePacerPtr createFramePacer() noexcept;

        virtual RHICapabilities getCapabilities() const noexcept = 0;

        virtual void submit(RHICommandList&, RHISwapchain* swapchain = nullptr) noexcept = 0;

        // for UI
        virtual void* getNative() noexcept = 0;
        // DeviceContext for D3D11, CommandQueue for D3D12
        // not used at Metal
        virtual void* getContextOrQueue() noexcept{ return nullptr; };
    };
#endif

    using RHIDevicePtr = std::unique_ptr<RHIDevice>;

    // each platform should implement this function
    RHIDevicePtr createDevice() noexcept;
}