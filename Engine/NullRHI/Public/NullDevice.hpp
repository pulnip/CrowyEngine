#pragma once

#include "RHIAPI.h"
#ifdef USE_STATIC_RHI
    #include "RHIDefinitions.h"
#else
    #include "RHIDevice.hpp"
#endif

namespace Crowy
{
    class NullDevice
#ifndef USE_STATIC_RHI
        : public RHIDevice
#endif
    {
    public:
        NullDevice() = default;
        ~NullDevice() = default;

        RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) RHI_OVERRIDE;
        RHITexturePtr createTexture(const RHITextureCreateDesc&) RHI_OVERRIDE;
        RHIShaderPtr  createShader (const RHIShaderCreateDesc& ) RHI_OVERRIDE;

        RHICapabilities getCapabilities() const RHI_OVERRIDE;
    };
}
