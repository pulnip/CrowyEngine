#pragma once

#include <memory>
#include "RHIAPI.h"
#ifdef USE_STATIC_RHI
    #include "RHIDefinitions.h"
#else
    #include "RHIDevice.hpp"
#endif

namespace Crowy
{
    class MetalDevice
#ifndef USE_STATIC_RHI
        : public RHIDevice
#endif
    {
    public:
        MetalDevice();
        ~MetalDevice();

        RHIBufferPtr  createBuffer (const RHIBufferCreateDesc& ) RHI_OVERRIDE;
        RHITexturePtr createTexture(const RHITextureCreateDesc&) RHI_OVERRIDE;

        RHICapabilities getCapabilities() const RHI_OVERRIDE;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
