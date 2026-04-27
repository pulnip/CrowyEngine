#include "NullBuffer.hpp"
#include "NullBufferView.hpp"
#include "NullCommandList.hpp"
#include "NullDevice.hpp"
#include "NullFence.hpp"
#include "NullPipelineState.hpp"
#include "NullSampler.hpp"
#include "NullShader.hpp"
#include "NullSwapchain.hpp"
#include "NullTexture.hpp"
#include "NullTextureView.hpp"
#include "RHIFrameScope.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<NullDevice> createDevice() noexcept{
        return std::make_unique<NullDevice>();
    }
#else
    RHIDevicePtr createDevice() noexcept{
        return std::make_unique<NullDevice>();
    }
#endif

    RHIFrameScopePtr NullDevice::createFrameScope() noexcept{
        return std::make_unique<RHIFrameScope>();
    }

    RHIBufferPtr NullDevice::createBuffer(
        const RHIBufferCreateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullBuffer>(desc, name);
    }

    RHIBufferViewPtr NullDevice::createBufferView(
        const RHIBuffer&,
        const RHIBufferViewDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullBufferView>(desc, name);
    }

    RHITexturePtr NullDevice::createTexture(
        const RHITextureCreateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullTexture>(desc, name);
    }

    RHITextureViewPtr NullDevice::createTextureView(
        const RHITexture&,
        const RHITextureViewDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullTextureView>(desc, name);
    }

    RHIShaderPtr NullDevice::createShader(
        const RHIShaderCreateDesc& desc
    ){
        return std::make_unique<NullShader>(desc);
    }
    RHISamplerPtr NullDevice::createSampler(
        const RHISamplerState& desc
    ) noexcept{
        return std::make_unique<NullSampler>(desc);
    }

    RHIGraphicsPipelineStatePtr NullDevice::createPipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullGraphicsPipelineState>(desc, name);
    }

    RHIComputePipelineStatePtr NullDevice::createPipelineState(
        const RHIComputePipelineStateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullComputePipelineState>(desc, name);
    }

    RHISwapchainPtr NullDevice::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return std::make_unique<NullSwapchain>(desc);
    }

    RHIFencePtr NullDevice::createFence(uint64_t initialValue) noexcept{
        return std::make_unique<NullFence>(initialValue);
    }

    RHICommandListPtr NullDevice::createCommandList() noexcept{
        return std::make_unique<NullCommandList>();
    }

    RHICapabilities NullDevice::getCapabilities() const noexcept{
        return {};
    }

    void NullDevice::submit(RHICommandList&, RHISwapchain*) noexcept{

    }
}