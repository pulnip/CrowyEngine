#include "NullBuffer.hpp"
#include "NullCommandList.hpp"
#include "NullDevice.hpp"
#include "NullFence.hpp"
#include "NullPipelineState.hpp"
#include "NullShader.hpp"
#include "NullSwapchain.hpp"
#include "NullTexture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<NullDevice> createDevice(){
        return std::make_unique<NullDevice>();
    }
#else
    RHIDevicePtr createDevice(){
        return std::make_unique<NullDevice>();
    }
#endif

    RHIBufferPtr NullDevice::createBuffer(
        const RHIBufferCreateDesc& desc
    ){
        return std::make_unique<NullBuffer>(desc);
    }

    RHITexturePtr NullDevice::createTexture(
        const RHITextureCreateDesc& desc
    ){
        return std::make_unique<NullTexture>(desc);
    }

    RHIShaderPtr NullDevice::createShader(
        const RHIShaderCreateDesc& desc
    ){
        return std::make_unique<NullShader>(desc);
    }

    RHIPipelineStatePtr NullDevice::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ){
        return std::make_unique<NullPipelineState>(desc);
    }

    RHIPipelineStatePtr NullDevice::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ){
        return std::make_unique<NullPipelineState>(desc);
    }

    RHISwapchainPtr NullDevice::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ){
        return std::make_unique<NullSwapchain>(desc);
    }

    RHIFencePtr NullDevice::createFence(uint64_t initialValue){
        return std::make_unique<NullFence>(initialValue);
    }

    RHICommandListPtr NullDevice::createCommandList(){
        return std::make_unique<NullCommandList>();
    }

    RHICapabilities NullDevice::getCapabilities() const{
        return {};
    }

    void NullDevice::submit(
        RHICommandList*,
        RHISwapchain* presentTarget
    ){

    }
}