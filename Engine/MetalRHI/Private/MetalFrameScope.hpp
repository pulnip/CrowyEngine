#pragma once

#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIFrameScope.hpp"
#endif
#include "AutoreleasePoolScope.hpp"

namespace Crowy
{
    class MetalFrameScope
#ifndef USE_STATIC_RHI
        : public RHIFrameScope
#endif
    {
    private:
        AutoreleasePoolScope autoreleasePool;

    public:
        MetalFrameScope() = default;
        ~MetalFrameScope() = default;
    };
}