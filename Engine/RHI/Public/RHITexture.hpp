#pragma once

#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.h"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalTexture.hpp"
    #endif
#endif

namespace Crowy
{
    // GPU texture resource (1D, 2D, 3D, Cube, arrays)
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHITextureType = requires(T texture){
    };
    static_assert(RHITextureType<RHITexture>);
#else
    class RHITexture{
    public:
        DECLARE_INTERFACE(RHITexture)

        // Platform-specific resource getter (for interop)
        virtual void* getNativeResource() = 0;
    };
#endif
}
