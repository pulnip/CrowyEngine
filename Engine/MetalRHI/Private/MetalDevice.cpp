#include "MetalDevice.hpp"
#include "MetalRHIDefinitions.h"

#ifdef __APPLE__

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
#ifdef USE_STATIC_RHI
    std::unique_ptr<MetalDevice> createDevice(){
        return std::make_unique<MetalDevice>();
    }
#else
    std::unique_ptr<RHIDevice> createDevice(){
        return std::make_unique<MetalDevice>();
    }
#endif

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