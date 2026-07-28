#pragma once

#include <Metal/MTLSampler.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{

    class MetalSampler final{
    private:
        NS::SharedPtr<MTL::SamplerState> sampler;

    public:
        MetalSampler(
            MTL::Device& device,
            const RHISamplerState& desc
        );

        ~MetalSampler();

        auto Get(){ return sampler.get(); }
    };
}
