#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISampler.hpp"
#endif

namespace Crowy
{
    class NullSampler
#ifndef USE_STATIC_RHI
        : public RHISampler
#endif
    {
    public:
        NullSampler(
            const RHISamplerState& desc
        ){

        }
    };
}