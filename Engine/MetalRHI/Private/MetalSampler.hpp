#pragma once

#include <vector>

#include <Metal/MTLSampler.hpp>

#include "MetalDefinitions.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"

namespace Crowy
{

    class MetalSampler final {
    private:
        RefPtr<MTL::SamplerState> sampler;

    public:
        MetalSampler(MTL::Device& device, const RHISamplerState& desc);

        ~MetalSampler();
        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(MetalSampler)

        auto Get() { return sampler.get(); }
    };

    // slang cannot express Metal static samplers, so the StaticSamplers table
    // is bound per encoder at the indices of the Sampler.slang globals
    class MetalReservedSamplers final {
    private:
        std::vector<MetalSampler> samplers;

    public:
        explicit MetalReservedSamplers(MTL::Device& device) {
            samplers.reserve(StaticSamplers.size());
            for(const auto& desc: StaticSamplers) {
                samplers.emplace_back(device, desc);
            }
        }

        // samplerIndex: index into StaticSamplers
        MTL::SamplerState* Get(u32 samplerIndex) {
            return samplers[samplerIndex].Get();
        }
    };
}
