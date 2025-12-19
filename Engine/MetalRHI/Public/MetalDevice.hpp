#pragma once

#if defined(__APPLE__)

#include <memory>
#include "RHIDevice.hpp"

namespace Crowy
{
    class MetalDevice: public RHIDevice{
    public:
        MetalDevice();
        ~MetalDevice();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}

#endif