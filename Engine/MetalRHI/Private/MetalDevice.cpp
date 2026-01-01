#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
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
    std::unique_ptr<MetalDevice> createDevice(){
        return std::make_unique<MetalDevice>();
    }
#else
    RHIDevicePtr createDevice(){
        return std::make_unique<MetalDevice>();
    }
#endif

    struct MetalDevice::Impl{
        MTL::Device* device;
        MTL::CommandQueue* commandQueue;

        MTL::SamplerState* defaultSampler;

        MTL::CommandBuffer* pendingCommandBuffer = nullptr;

        Impl(){
            device = MTL::CreateSystemDefaultDevice();
            if(!device){
                throw std::runtime_error("No GPU available");
            }

            commandQueue = device->newCommandQueue();
            if(!commandQueue){
                throw std::runtime_error("Failed to create command queue");
            }

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

            if(!defaultSampler){
                throw std::runtime_error("Failed to create default sampler");
            }
        }

        ~Impl(){
            defaultSampler->release();
            commandQueue->release();
            device->release();
        }

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc& desc
        ){
            return std::make_unique<MetalBuffer>(device, desc);
        }

        RHITexturePtr createTexture(
            const RHITextureCreateDesc& desc
        ){
            return std::make_unique<MetalTexture>(device, desc);
        }

        RHIShaderPtr createShader(
            const RHIShaderCreateDesc& desc
        ){
            return std::make_unique<MetalShader>(device, desc);
        }

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ){
            return std::make_unique<MetalPipelineState>(device, desc);
        }

        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc& desc
        ){
            return std::make_unique<MetalPipelineState>(device, desc);
        }

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc& desc
        ){
            return std::make_unique<MetalSwapchain>(device, desc);
        }

        RHICommandListPtr createCommandList(){
            return std::make_unique<MetalCommandList>(commandQueue, defaultSampler);
        }

        RHIFencePtr createFence(uint64_t initialValue){
            return std::make_unique<MetalFence>(device, initialValue);
        }

        void submit(RHICommandList* cmdList, RHISwapchain* swapchain){
            auto mtlCmdList = static_cast<MetalCommandList*>(cmdList);
            auto cmdBuffer = mtlCmdList->getCommandBuffer();

            if(!cmdBuffer) return;

            if(swapchain){
                auto mtlSwapchain = static_cast<MetalSwapchain*>(swapchain);
                auto drawable = mtlSwapchain->getCurrentDrawable();
                if(drawable)
                    cmdBuffer->presentDrawable(drawable);
            }

            cmdBuffer->commit();
        }
    };

    MetalDevice::MetalDevice()
        :impl(std::make_unique<Impl>()){}

    MetalDevice::~MetalDevice(){}

    RHIBufferPtr MetalDevice::createBuffer(
        const RHIBufferCreateDesc& desc
    ){
        return impl->createBuffer(desc);
    }

    RHITexturePtr MetalDevice::createTexture(
        const RHITextureCreateDesc& desc
    ){
        return impl->createTexture(desc);
    }

    RHIShaderPtr MetalDevice::createShader(
        const RHIShaderCreateDesc& desc
    ){
        return impl->createShader(desc);
    }

    RHIPipelineStatePtr MetalDevice::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ){
        return impl->createGraphicsPipelineState(desc);
    }

    RHIPipelineStatePtr MetalDevice::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ){
        return impl->createComputePipelineState(desc);
    }

    RHISwapchainPtr MetalDevice::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ){
        return impl->createSwapchain(desc);
    }

    RHICommandListPtr MetalDevice::createCommandList(){
        return impl->createCommandList();
    }

    RHIFencePtr MetalDevice::createFence(uint64_t initialValue){
        return impl->createFence(initialValue);
    }

    RHICapabilities MetalDevice::getCapabilities() const{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void MetalDevice::submit(RHICommandList* cmdList, RHISwapchain* swapchain){
        impl->submit(cmdList, swapchain);
    }
}