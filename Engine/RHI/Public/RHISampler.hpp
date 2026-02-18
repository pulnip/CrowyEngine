#pragma once

#include <memory>
#include "RHIFWD.hpp"
#include "semantics.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalSampler.hpp"
    #else
        #include "NullSampler.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHISamplerType = requires(T sampler){
    };
    static_assert(RHISamplerType<RHISampler>);
#else
    class RHISampler{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHISampler)
    };
#endif

    using RHISamplerPtr = std::unique_ptr<RHISampler>;
}
