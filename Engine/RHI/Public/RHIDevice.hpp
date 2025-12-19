#pragma once

#include <memory>

namespace Crowy
{
    class RHIDevice{
    public:
        virtual ~RHIDevice() = default;
    };

    // each platform should implement this function
    std::unique_ptr<RHIDevice> createDevice();
}