#include "NullBuffer.hpp"
#include "NullCommandList.hpp"
#include "NullDevice.hpp"
#include "NullFence.hpp"
#include "NullPipelineState.hpp"
#include "NullSampler.hpp"
#include "NullShader.hpp"
#include "NullSwapchain.hpp"
#include "NullTexture.hpp"

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

    RHIBufferPtr NullDevice::createBuffer(
        const RHIBufferCreateDesc& desc
    ) noexcept{
        return std::make_unique<NullBuffer>(desc);
    }

    RHITexturePtr NullDevice::createTexture(
        const RHITextureCreateDesc& desc
    ) noexcept{
        return std::make_unique<NullTexture>(desc);
    }

    RHIShaderPtr NullDevice::createShader(
        const RHIShaderCreateDesc& desc
    ) noexcept{
        return std::make_unique<NullShader>(desc);
    }
    RHISamplerPtr NullDevice::createSampler(
        const RHISamplerState& desc
    ) noexcept{
        return std::make_unique<NullSampler>(desc);
    }

    RHIPipelineStatePtr NullDevice::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ) noexcept{
        return std::make_unique<NullPipelineState>(desc);
    }

    RHIPipelineStatePtr NullDevice::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ) noexcept{
        return std::make_unique<NullPipelineState>(desc);
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

    void NullDevice::submit(RHICommandList&, RHISwapchain&) noexcept{

    }
}