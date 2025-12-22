#include "MetalDevice.hpp"
#include "MetalRHIDefinitions.h"

#if defined(__APPLE__)

// MetalDevice C Bridge Functions
extern "C"{
MetalDevicePtr MetalDevice_create(
);
void MetalDevice_destroy(
    MetalDevicePtr device
);
}

namespace Crowy
{
    std::unique_ptr<RHIDevice> createDevice(){
        return std::make_unique<MetalDevice>();
    }

    struct MetalDevice::Impl{
        MetalDevicePtr const devicePtr;

        Impl()
            :devicePtr(MetalDevice_create()){}

        ~Impl(){
            MetalDevice_destroy(devicePtr);
        }
    };

    MetalDevice::MetalDevice()
        :impl(std::make_unique<Impl>()){}

    MetalDevice::~MetalDevice(){}

    RHICapabilities MetalDevice::getCapabilities() const{
        return { .flipTextureV = true, .clipSpaceMinZ = 0.0f };
    }
}

#endif