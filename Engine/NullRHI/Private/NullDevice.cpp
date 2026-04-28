#include "NullBuffer.hpp"
#include "NullCommandList.hpp"
#include "NullDevice.hpp"
#include "NullFence.hpp"
#include "NullPipelineState.hpp"
#include "NullSampler.hpp"
#include "NullSwapchain.hpp"
#include "NullTexture.hpp"
#include "RHIFrameScope.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<NullDevice> createDevice() noexcept{
        return std::make_unique<NullDevice>();
    }
#else
    RHIDeviceRAII createDevice() noexcept{
        return std::make_unique<NullDevice>();
    }
#endif

    RHIFrameScopeRAII NullDevice::createFrameScope() noexcept{
        return std::make_unique<RHIFrameScope>();
    }

    RHIBufferRAII NullDevice::createBuffer(
        const RHIBufferCreateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullBuffer>(desc, name);
    }

    RHITextureRAII NullDevice::createTexture(
        const RHITextureCreateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullTexture>(desc, name);
    }

    RHISamplerRAII NullDevice::createSampler(
        const RHISamplerState& desc
    ) noexcept{
        return std::make_unique<NullSampler>(desc);
    }

    RHIGraphicsPipelineStateRAII NullDevice::createPipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullGraphicsPipelineState>(desc, name);
    }

    RHIComputePipelineStateRAII NullDevice::createPipelineState(
        const RHIComputePipelineStateDesc& desc,
        const std::string& name
    ) noexcept{
        return std::make_unique<NullComputePipelineState>(desc, name);
    }

    RHISwapchainRAII NullDevice::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return std::make_unique<NullSwapchain>(desc);
    }

    RHIFenceRAII NullDevice::createFence(uint64_t initialValue) noexcept{
        return std::make_unique<NullFence>(initialValue);
    }

    RHICommandListRAII NullDevice::createCommandList() noexcept{
        return std::make_unique<NullCommandList>();
    }

    RHICapabilities NullDevice::getCapabilities() const noexcept{
        return {};
    }

    void NullDevice::submit(RHICommandList&, RHISwapchain*) noexcept{

    }
}