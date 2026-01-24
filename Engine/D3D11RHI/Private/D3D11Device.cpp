#include "D3D11Buffer.hpp"
#include "D3D11CommandList.hpp"
#include "D3D11Device.hpp"
#include "D3D11Fence.hpp"
#include "D3D11PipelineState.hpp"
#include "D3D11Shader.hpp"
#include "D3D11Swapchain.hpp"
#include "D3D11Texture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<D3D11Device> createDevice() noexcept{
        return std::make_unique<D3D11Device>();
    }
#else
    RHIDevicePtr createDevice() noexcept{
        return std::make_unique<D3D11Device>();
    }
#endif

    RHIBufferPtr D3D11Device::createBuffer(
        const RHIBufferCreateDesc& desc
    ) noexcept{
        return std::make_unique<D3D11Buffer>(desc);
    }

    RHITexturePtr D3D11Device::createTexture(
        const RHITextureCreateDesc& desc
    ) noexcept{
        return std::make_unique<D3D11Texture>(desc);
    }

    RHIShaderPtr D3D11Device::createShader(
        const RHIShaderCreateDesc& desc
    ) noexcept{
        return std::make_unique<D3D11Shader>(desc);
    }

    RHIPipelineStatePtr D3D11Device::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ) noexcept{
        return std::make_unique<D3D11PipelineState>(desc);
    }

    RHIPipelineStatePtr D3D11Device::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ) noexcept{
        return std::make_unique<D3D11PipelineState>(desc);
    }

    RHISwapchainPtr D3D11Device::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return std::make_unique<D3D11Swapchain>(desc);
    }

    RHIFencePtr D3D11Device::createFence(uint64_t initialValue) noexcept{
        return std::make_unique<D3D11Fence>(initialValue);
    }

    RHICommandListPtr D3D11Device::createCommandList() noexcept{
        return std::make_unique<D3D11CommandList>();
    }

    RHICapabilities D3D11Device::getCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void D3D11Device::submit(RHICommandList&, RHISwapchain&) noexcept{

    }
}