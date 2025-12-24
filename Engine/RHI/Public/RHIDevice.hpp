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
        { device.createBuffer() } -> std::same_as<std::unique_ptr<RHIBuffer>>;
    };
    static_assert(RHIDeviceType<RHIDevice>);
#else
    class RHIDevice{
    public:
        RHIDevice() = default;
        virtual ~RHIDevice() = default;
        RHIDevice(const RHIDevice&) = delete;
        RHIDevice(RHIDevice&&) = default;
        RHIDevice& operator=(const RHIDevice&) = delete;
        RHIDevice& operator=(RHIDevice&&) = default;

        virtual std::unique_ptr<RHIBuffer> createBuffer(const RHIBufferCreateDesc&) = 0;
        virtual std::unique_ptr<RHITexture> createTexture(const RHITextureCreateDesc&) = 0;

        virtual RHICapabilities getCapabilities() const = 0;
    };
#endif

    // each platform should implement this function
    std::unique_ptr<RHIDevice> createDevice();
}