#pragma once

#include <memory>
#include "RHIFWD.hpp"
#include "semantics.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalFrameScope.hpp"
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

    using RHIFrameScopePtr = std::unique_ptr<RHIFrameScope>;
}
