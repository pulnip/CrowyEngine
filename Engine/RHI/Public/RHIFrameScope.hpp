#pragma once

#include "RHIFWD.hpp"
#include "semantics.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalFrameScope.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11FrameScope.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIFrameScopeType = requires(T frameScope){
    };
    static_assert(RHIFrameScopeType<RHIFrameScope>);
#else
    class RHIFrameScope{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIFrameScope)
    };
#endif
}
