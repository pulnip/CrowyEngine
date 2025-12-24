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

        std::unique_ptr<RHIBuffer> createBuffer(const RHIBufferCreateDesc&) RHI_OVERRIDE;
        std::unique_ptr<RHITexture> createTexture(const RHITextureCreateDesc&) RHI_OVERRIDE;

        RHICapabilities getCapabilities() const RHI_OVERRIDE;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
