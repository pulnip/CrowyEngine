#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include "MetalBuffer.hpp"
#include "MetalDevice.hpp"
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
    };

    MetalDevice::MetalDevice()
        :impl(std::make_unique<Impl>()){}

    MetalDevice::~MetalDevice(){}

    RHIBufferPtr MetalDevice::createBuffer(
        const RHIBufferCreateDesc& desc
    ){
        return impl->createBuffer(desc);
    }

    RHICapabilities MetalDevice::getCapabilities() const{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }
}