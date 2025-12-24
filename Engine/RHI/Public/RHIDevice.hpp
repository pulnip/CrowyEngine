#pragma once

#include <memory>
#include "semantics.hpp"
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
        { device.createBuffer() } -> std::same_as<std::unique_ptr<RHIBuffer>>;
    };
    static_assert(RHIDeviceType<RHIDevice>);
#else
    class RHIDevice{
    public:
        DECLARE_INTERFACE(RHIDevice)

        virtual RHIBufferPtr  createBuffer(const RHIBufferCreateDesc&)   = 0;
        virtual RHITexturePtr createTexture(const RHITextureCreateDesc&) = 0;

        virtual RHICapabilities getCapabilities() const = 0;
    };
#endif

    // each platform should implement this function
    RHIDevicePtr createDevice();
}