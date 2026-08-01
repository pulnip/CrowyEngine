#pragma once

#include <vector>
#include <Metal/MTLSampler.hpp>
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"

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
        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(MetalSampler)

        auto Get(){ return sampler.get(); }
    };

    // the RHI_STATIC_SAMPLERS table;
    // slang cannot express Metal static samplers,
    // so bound per encoder at the fixed
    // indices assigned to the Sampler.slang globals
    class MetalReservedSamplers final{
    private:
        std::vector<MetalSampler> samplers;

    public:
        explicit MetalReservedSamplers(MTL::Device& device){
            samplers.reserve(RHI_STATIC_SAMPLERS.size());
            for(const auto& desc: RHI_STATIC_SAMPLERS){
                samplers.emplace_back(device, desc);
            }
        }

        // samplerIndex: index into RHI_STATIC_SAMPLERS
        MTL::SamplerState* Get(u32 samplerIndex){
            return samplers[samplerIndex].Get();
        }
    };
}
