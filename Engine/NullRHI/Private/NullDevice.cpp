#include "NullBuffer.hpp"
#include "NullDevice.hpp"
#include "NullTexture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<NullDevice> createDevice(){
        return std::make_unique<NullDevice>();
    }
#else
    RHIDevicePtr createDevice(){
        return std::make_unique<NullDevice>();
    }
#endif

    RHIBufferPtr NullDevice::createBuffer(
        const RHIBufferCreateDesc& desc
    ){
        return std::make_unique<NullBuffer>(desc);
    }

    RHITexturePtr NullDevice::createTexture(
        const RHITextureCreateDesc& desc
    ){
        return std::make_unique<NullTexture>(desc);
    }

    RHICapabilities NullDevice::getCapabilities() const{
        return {};
    }
}