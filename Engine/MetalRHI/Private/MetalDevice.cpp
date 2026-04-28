extern "C"{
    // Debug AutoreleasePool
    void _objc_autoreleasePoolPrint(void);
}

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include "assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalDevice.hpp"
#include "MetalFence.hpp"
#include "MetalFrameScope.hpp"
#include "MetalPipelineState.hpp"
#include "MetalSampler.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<MetalDevice> createDevice() noexcept{
        return std::make_unique<MetalDevice>();
    }
#else
    RHIDeviceRAII createDevice() noexcept{
        return std::make_unique<MetalDevice>();
    }
#endif

    struct MetalDevice::Impl{
        MTL::Device* device;
        MTL::CommandQueue* commandQueue;

        MTL::CommandBuffer* pendingCommandBuffer = nullptr;

        AutoreleasePoolScope autoreleasePool;

        Impl() noexcept{
            device = MTL::CreateSystemDefaultDevice();
            CROWY_ASSERT(device != nullptr, "No GPU Available");

            commandQueue = device->newCommandQueue();
            CROWY_ASSERT(commandQueue != nullptr,
                "Failed to create command queue"
            );
        }

        ~Impl(){
            if(commandQueue != nullptr)
                commandQueue->release();
            if(device != nullptr)
                device->release();

            // _objc_autoreleasePoolPrint();
        }

        RHIFrameScopeRAII createFrameScope() noexcept{
            return std::make_unique<MetalFrameScope>();
        }

        RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc& desc,
            const std::string& name
        ) noexcept{
            return std::make_unique<MetalBuffer>(*device, desc, name);
        }

        RHITextureRAII createTexture(
            const RHITextureCreateDesc& desc,
            const std::string& name
        ) noexcept{
            return std::make_unique<MetalTexture>(*device, desc, name);
        }

        RHISamplerRAII createSampler(
            const RHISamplerState& desc
        ) noexcept{
            return std::make_unique<MetalSampler>(*device, desc);
        }

        RHIGraphicsPipelineStateRAII createPipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        ) noexcept{
            return std::make_unique<MetalGraphicsPipelineState>(*device, desc, name);
        }

        RHIComputePipelineStateRAII createPipelineState(
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        ) noexcept{
            return std::make_unique<MetalComputePipelineState>(*device, desc, name);
        }

        RHISwapchainRAII createSwapchain(
            const RHISwapchainCreateDesc& desc
        ) noexcept{
            return std::make_unique<MetalSwapchain>(*device, desc);
        }

        RHICommandListRAII createCommandList() noexcept{
            return std::make_unique<MetalCommandList>(commandQueue);
        }

        RHIFenceRAII createFence(uint64_t initialValue) noexcept{
            return std::make_unique<MetalFence>(*device, initialValue);
        }

        void submit(RHICommandList& cmdList, RHISwapchain* swapchain) noexcept{
            auto& mtlCmdList = static_cast<MetalCommandList&>(cmdList);
            auto cmdBuffer = mtlCmdList.get();
            if(swapchain != nullptr){
                auto& mtlSwapchain = static_cast<MetalSwapchain&>(*swapchain);
                auto drawable = mtlSwapchain.getCurrentDrawable();

                cmdBuffer->presentDrawable(drawable);
            }

            cmdBuffer->commit();
        }

        MTL::Device* get() noexcept{ return device; }
    };

    MetalDevice::MetalDevice() noexcept
        :impl(std::make_unique<Impl>()){}

    MetalDevice::~MetalDevice(){}

    RHIFrameScopeRAII MetalDevice::createFrameScope() noexcept{
        return impl->createFrameScope();
    }

    RHIBufferRAII MetalDevice::createBuffer(
        const RHIBufferCreateDesc& desc,
        const std::string& name
    ) noexcept{
        return impl->createBuffer(desc, name);
    }

    RHITextureRAII MetalDevice::createTexture(
        const RHITextureCreateDesc& desc,
        const std::string& name
    ) noexcept{
        return impl->createTexture(desc, name);
    }

    RHISamplerRAII MetalDevice::createSampler(
        const RHISamplerState& desc
    ) noexcept{
        return impl->createSampler(desc);
    }

    RHIGraphicsPipelineStateRAII MetalDevice::createPipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        const std::string& name
    ) noexcept{
        return impl->createPipelineState(desc, name);
    }

    RHIComputePipelineStateRAII MetalDevice::createPipelineState(
        const RHIComputePipelineStateDesc& desc,
        const std::string& name
    ) noexcept{
        return impl->createPipelineState(desc, name);
    }

    RHISwapchainRAII MetalDevice::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return impl->createSwapchain(desc);
    }

    RHICommandListRAII MetalDevice::createCommandList() noexcept{
        return impl->createCommandList();
    }

    RHIFenceRAII MetalDevice::createFence(uint64_t initialValue) noexcept{
        return impl->createFence(initialValue);
    }

    RHICapabilities MetalDevice::getCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void MetalDevice::submit(RHICommandList& cmdList, RHISwapchain* swapchain) noexcept{
        impl->submit(cmdList, swapchain);
    }

    void* MetalDevice::getNative() noexcept{
        return impl->get();
    }
}