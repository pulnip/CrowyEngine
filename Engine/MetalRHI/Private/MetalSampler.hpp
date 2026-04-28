#pragma once

#include <utility>
#include <Metal/Metal.hpp>
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISampler.hpp"
#endif
#include "MetalUtil.hpp"

namespace Crowy
{
    inline auto convert(RHIAddressMode mode){
        switch(mode){
        case RHIAddressMode::Wrap  : return MTL::SamplerAddressModeRepeat;
        case RHIAddressMode::Clamp : return MTL::SamplerAddressModeClampToEdge;
        case RHIAddressMode::Mirror: return MTL::SamplerAddressModeMirrorRepeat;
        case RHIAddressMode::Border: return MTL::SamplerAddressModeClampToBorderColor;
        default:
            std::unreachable();
        }
    }

    inline auto convertMinMagFilter(RHIFilter filter){
        switch(filter){
        case RHIFilter::Nearest: return MTL::SamplerMinMagFilterNearest;
        case RHIFilter::Linear:  return MTL::SamplerMinMagFilterLinear;
        default:
            std::unreachable();
        }
    }

    inline auto convertMipFilter(RHIFilter filter){
        switch(filter){
        case RHIFilter::Nearest: return MTL::SamplerMipFilterNearest;
        case RHIFilter::Linear:  return MTL::SamplerMipFilterLinear;
        default:
            std::unreachable();
        }
    }

    class MetalSampler
#ifndef USE_STATIC_RHI
        : public RHISampler
#endif
    {
    private:
        MTL::SamplerState* sampler = nullptr;

    public:
        MetalSampler(
            MTL::Device& device,
            const RHISamplerState& desc
        ){
            auto samplerDesc = MTL::SamplerDescriptor::alloc()->init();
            samplerDesc->setMinFilter(convertMinMagFilter(desc.minFilter));
            samplerDesc->setMagFilter(convertMinMagFilter(desc.magFilter));
            samplerDesc->setMipFilter(convertMipFilter(desc.magFilter));
            samplerDesc->setSAddressMode(convert(desc.addressU));
            samplerDesc->setTAddressMode(convert(desc.addressV));
            samplerDesc->setRAddressMode(convert(desc.addressW));
            samplerDesc->setLodMinClamp(desc.minLOD);
            samplerDesc->setLodMaxClamp(desc.maxLOD);
            samplerDesc->setMaxAnisotropy(desc.maxAnisotropy);
            samplerDesc->setCompareFunction(convert(desc.compareFunc));

            sampler = device.newSamplerState(samplerDesc);
            samplerDesc->release();
        }

        ~MetalSampler(){
            sampler->release();
        }

        MTL::SamplerState* get() const{ return sampler; }
    };
}