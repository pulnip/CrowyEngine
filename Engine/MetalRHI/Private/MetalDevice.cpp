#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include "assert.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalDevice.hpp"
#include "MetalFence.hpp"
#include "MetalPipelineState.hpp"
#include "MetalShader.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<MetalDevice> createDevice() noexcept{
        return std::make_unique<MetalDevice>();
    }
#else
    RHIDevicePtr createDevice() noexcept{
        return std::make_unique<MetalDevice>();
    }
#endif

    struct MetalDevice::Impl{
        MTL::Device* device;
        MTL::CommandQueue* commandQueue;

        MTL::SamplerState* defaultSampler;

        MTL::CommandBuffer* pendingCommandBuffer = nullptr;

        Impl() noexcept{
            device = MTL::CreateSystemDefaultDevice();
            CROWY_ASSERT(device != nullptr, "No GPU Available");

            commandQueue = device->newCommandQueue();
            CROWY_ASSERT(commandQueue != nullptr,
                "Failed to create command queue"
            );

            auto samplerDesc = MTL::SamplerDescriptor::alloc()->init();
            samplerDesc->setMinFilter(MTL::SamplerMinMagFilterLinear);
            samplerDesc->setMagFilter(MTL::SamplerMinMagFilterLinear);
            samplerDesc->setMipFilter(MTL::SamplerMipFilterLinear);
            samplerDesc->setSAddressMode(MTL::SamplerAddressModeRepeat);
            samplerDesc->setTAddressMode(MTL::SamplerAddressModeRepeat);
            samplerDesc->setRAddressMode(MTL::SamplerAddressModeRepeat);
            samplerDesc->setMaxAnisotropy(16);

            defaultSampler = device->newSamplerState(samplerDesc);
            samplerDesc->release();

            CROWY_ASSERT(defaultSampler != nullptr,
                "Failed to create default sampler"
            );
        }

        ~Impl(){
            if(defaultSampler != nullptr)
                defaultSampler->release();
            if(commandQueue != nullptr)
                commandQueue->release();
            if(device != nullptr)
                device->release();
        }

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc& desc
        ) noexcept{
            return std::make_unique<MetalBuffer>(device, desc);
        }

        RHITexturePtr createTexture(
            const RHITextureCreateDesc& desc
        ) noexcept{
            return std::make_unique<MetalTexture>(device, desc);
        }

        RHIShaderPtr createShader(
            const RHIShaderCreateDesc& desc
        ) noexcept{
            return std::make_unique<MetalShader>(device, desc);
        }

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ) noexcept{
            return std::make_unique<MetalPipelineState>(device, desc);
        }

        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc& desc
        ) noexcept{
            return std::make_unique<MetalPipelineState>(device, desc);
        }

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc& desc
        ) noexcept{
            return std::make_unique<MetalSwapchain>(device, desc);
        }

        RHICommandListPtr createCommandList() noexcept{
            return std::make_unique<MetalCommandList>(commandQueue, defaultSampler);
        }

        RHIFencePtr createFence(uint64_t initialValue) noexcept{
            return std::make_unique<MetalFence>(device, initialValue);
        }

        void submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
            auto& mtlCmdList = static_cast<MetalCommandList&>(cmdList);
            auto& mtlSwapchain = static_cast<MetalSwapchain&>(swapchain);
            auto cmdBuffer = mtlCmdList.getCommandBuffer();
            auto drawable = mtlSwapchain.getCurrentDrawable();

            cmdBuffer->presentDrawable(drawable);
            cmdBuffer->commit();
        }

        MTL::Device* getNative() noexcept{ return device; }
    };

    MetalDevice::MetalDevice() noexcept
        :impl(std::make_unique<Impl>()){}

    MetalDevice::~MetalDevice(){}

    RHIBufferPtr MetalDevice::createBuffer(
        const RHIBufferCreateDesc& desc
    ) noexcept{
        return impl->createBuffer(desc);
    }

    RHITexturePtr MetalDevice::createTexture(
        const RHITextureCreateDesc& desc
    ) noexcept{
        return impl->createTexture(desc);
    }

    RHIShaderPtr MetalDevice::createShader(
        const RHIShaderCreateDesc& desc
    ) noexcept{
        return impl->createShader(desc);
    }

    RHIPipelineStatePtr MetalDevice::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ) noexcept{
        return impl->createGraphicsPipelineState(desc);
    }

    RHIPipelineStatePtr MetalDevice::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ) noexcept{
        return impl->createComputePipelineState(desc);
    }

    RHISwapchainPtr MetalDevice::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return impl->createSwapchain(desc);
    }

    RHICommandListPtr MetalDevice::createCommandList() noexcept{
        return impl->createCommandList();
    }

    RHIFencePtr MetalDevice::createFence(uint64_t initialValue) noexcept{
        return impl->createFence(initialValue);
    }

    RHICapabilities MetalDevice::getCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void MetalDevice::submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
        impl->submit(cmdList, swapchain);
    }

    void* MetalDevice::getNative() noexcept{
        return impl->getNative();
    }
}