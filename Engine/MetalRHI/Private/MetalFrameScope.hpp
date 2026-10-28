#pragma once

#include "AutoreleasePoolScope.hpp"
#include "RHIFrameScope.hpp"

namespace Crowy
{
    class MetalFrameScope final: public RHIFrameScope {
    private:
        AutoreleasePoolScope autoreleasePool;

    public:
        MetalFrameScope() = default;
        ~MetalFrameScope() = default;

        CROWY_DECLARE_PINNED(MetalFrameScope)
    };
}
