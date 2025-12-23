#pragma once

#include <memory>
#include "RHIFWD.hpp"
#include "RHIDefinitions.h"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalDevice.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIDeviceType = requires(T device){
        { device.getCapabilities() } -> std::same_as<RHICapabilities>;
    };
    static_assert(RHIDeviceType<RHIDevice>);
#else
    class RHIDevice{
    public:
        virtual ~RHIDevice() = default;

        virtual RHICapabilities getCapabilities() const = 0;
    };
#endif

    // each platform should implement this function
    std::unique_ptr<RHIDevice> createDevice();
}