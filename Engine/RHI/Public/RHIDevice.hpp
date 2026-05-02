#pragma once

#include <string>
#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIFence.hpp"
#include "RHIFrameScope.hpp"
#include "RHIPipelineState.hpp"
#include "RHISampler.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalDevice.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11Device.hpp"
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
        RHIGraphicsPipelineStateDesc gpsDesc,
        RHIComputePipelineStateDesc cpsDesc
    ){
        { device.createBuffer(bufDesc) } -> std::same_as<RHIBufferRAII>;
        { device.createTexture(texDesc) } -> std::same_as<RHITextureRAII>;

        { device.createGraphicsPipelineState(gpsDesc) } -> std::same_as<RHIPipelineStateRAII>;
        { device.createComputePipelineState(cpsDesc) } -> std::same_as<RHIPipelineStateRAII>;

        { device.createFence(uint64_t(0)) } -> std::same_as<RHIFenceRAII>;

        { device.getCapabilities() } -> std::same_as<RHICapabilities>;
    };
    static_assert(RHIDeviceType<RHIDevice>);
#else
    class RHIDevice{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIDevice)

        virtual RHIFrameScopeRAII createFrameScope() = 0;

        virtual RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc&,
            const std::string& name = ""
        ) = 0;
        virtual RHITextureRAII createTexture(
            const RHITextureCreateDesc&,
            const std::string& name = ""
        ) = 0;
        virtual RHISamplerRAII createSampler(
            const RHISamplerState&
        ) = 0;

        virtual RHIGraphicsPipelineStateRAII createPipelineState(
            const RHIGraphicsPipelineStateDesc&,
            const std::string& name = ""
        ) = 0;
        virtual RHIComputePipelineStateRAII createPipelineState(
            const RHIComputePipelineStateDesc&,
            const std::string& name = ""
        ) = 0;

        virtual RHISwapchainRAII createSwapchain(
            const RHISwapchainCreateDesc&
        ) = 0;

        virtual RHICommandListRAII createCommandList() = 0;

        virtual RHIFenceRAII createFence(uint64_t initialValue = 0) = 0;

        FramePacerRAII createFramePacer() noexcept;

        virtual RHICapabilities getCapabilities() const noexcept = 0;

        virtual void submit(RHICommandList&, RHISwapchain* swapchain = nullptr) noexcept = 0;

        // for UI
        virtual void* getNative() noexcept = 0;
        // DeviceContext for D3D11, CommandQueue for D3D12
        // not used at Metal
        virtual void* getContextOrQueue() noexcept{ return nullptr; };
    };
#endif

    // each platform should implement this function
    RHIDeviceRAII createDevice();
}