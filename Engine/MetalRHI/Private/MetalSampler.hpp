#pragma once

#include <Metal/MTLSampler.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{

    class MetalSampler final{
    private:
        MTL::SamplerState* sampler = nullptr;

    public:
        MetalSampler(
            MTL::Device& device,
            const RHISamplerState& desc
        );

        ~MetalSampler();

        MTL::SamplerState* Get(){ return sampler; }
    };
}
