#pragma once

#include <memory>

namespace Crowy
{
    struct RHICapabilities{
        bool flipTextureV;
        float clipSpaceMinZ;
    };

    class RHIDevice{
    public:
        virtual ~RHIDevice() = default;

        virtual RHICapabilities getCapabilities() const = 0;
    };

    // each platform should implement this function
    std::unique_ptr<RHIDevice> createDevice();
}