#pragma once

#include <memory>
#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.h"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalDevice.hpp"
    #else
        #include "NullDevice.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIDeviceType = requires(T device,
        RHIBufferCreateDesc bufDesc,
        RHITextureCreateDesc texDesc,
        RHIShaderCreateDesc shaderDesc
    ){
        { device.createBuffer(bufDesc) } -> std::same_as<RHIBufferPtr>;
        { device.createTexture(texDesc) } -> std::same_as<RHITexturePtr>;
        { device.createShader(shaderDesc) } -> std::same_as<RHIShaderPtr>;

        { device.getCapabilities() } -> std::same_as<RHICapabilities>;
    };
    static_assert(RHIDeviceType<RHIDevice>);
#else
    class RHIDevice{
    public:
        DECLARE_INTERFACE(RHIDevice)

        virtual RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) = 0;
        virtual RHITexturePtr createTexture(const RHITextureCreateDesc&) = 0;
        virtual RHIShaderPtr  createShader (const RHIShaderCreateDesc& ) = 0;

        virtual RHICapabilities getCapabilities() const = 0;
    };
#endif

    // each platform should implement this function
    RHIDevicePtr createDevice();
}