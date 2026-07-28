#include <utility>
#include <Metal/MTLDevice.hpp>
#include "MetalSampler.hpp"
#include "MetalUtil.hpp"
#include "RHIDefinitions.hpp"

namespace{
    auto convert(Crowy::RHIAddressMode mode){
        using namespace Crowy;
        using enum RHIAddressMode;
        using namespace MTL;

        switch(mode){
        case Wrap  : return SamplerAddressModeRepeat;
        case Clamp : return SamplerAddressModeClampToEdge;
        case Mirror: return SamplerAddressModeMirrorRepeat;
        case Border: return SamplerAddressModeClampToBorderColor;
        default:
            std::unreachable();
        }
    }

    auto convertMinMagFilter(Crowy::RHIFilter filter){
        using namespace Crowy;
        using enum RHIFilter;
        using namespace MTL;

        switch(filter){
        case Nearest: return SamplerMinMagFilterNearest;
        case Linear:  return SamplerMinMagFilterLinear;
        default:
            std::unreachable();
        }
    }

    auto convertMipFilter(Crowy::RHIFilter filter){
        using namespace Crowy;
        using enum RHIFilter;
        using namespace MTL;

        switch(filter){
        case Nearest: return SamplerMipFilterNearest;
        case Linear:  return SamplerMipFilterLinear;
        default:
            std::unreachable();
        }
    }
}

namespace Crowy
{
    MetalSampler::MetalSampler(
        MTL::Device& device,
        const RHISamplerState& desc
    ){
        auto samplerDesc = MTL::SamplerDescriptor::alloc()->init();
        samplerDesc->setMinFilter(convertMinMagFilter(desc.minFilter));
        samplerDesc->setMagFilter(convertMinMagFilter(desc.magFilter));
        samplerDesc->setMipFilter(convertMipFilter(desc.magFilter));
        samplerDesc->setSAddressMode(::convert(desc.addressU));
        samplerDesc->setTAddressMode(::convert(desc.addressV));
        samplerDesc->setRAddressMode(::convert(desc.addressW));
        samplerDesc->setLodMinClamp(desc.minLOD);
        samplerDesc->setLodMaxClamp(desc.maxLOD);
        samplerDesc->setMaxAnisotropy(desc.maxAnisotropy);
        samplerDesc->setCompareFunction(convert(desc.compareFunc));

        sampler = NS::TransferPtr(
            device.newSamplerState(samplerDesc)
        );
        samplerDesc->release();
    }

    MetalSampler::~MetalSampler() = default;
}
