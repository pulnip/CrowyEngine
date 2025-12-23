#pragma once

#include <memory>
#ifdef USE_STATIC_RHI
    #include "RHIDefinitions.h"

    #define RHI_OVERRIDE
#else
    #include "RHIDevice.hpp"

    #define RHI_OVERRIDE override
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

        RHICapabilities getCapabilities() const RHI_OVERRIDE;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
