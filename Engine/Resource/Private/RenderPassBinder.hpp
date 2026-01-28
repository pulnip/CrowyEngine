#pragma once

#include <cstddef>
#include <limits>
#include <unordered_map>
#include <vector>
#include "BinderRegistry.hpp"
#include "RenderSpec.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct PlannedShader{
        ShaderSpec spec;
        size_t index = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };

    struct PlannedSampler{
        std::vector<RHISamplerState> spec;
        size_t index = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    using SamplerPresets = std::unordered_map<std::string, RHISamplerState>;

    struct RenderPassElementBindPlan{
        std::vector<PlannedShader> shaders;
        std::vector<PlannedSampler> samplers;
        std::vector<BindError> errors;
    };

    using RenderPassElementBinder = Binder<RenderPassElementBindPlan>;
    using RenderPassBinderRegistry = BinderRegistry<RenderPassElementBindPlan>;
    RenderPassBinderRegistry makeRenderPassBinderRegistry(const SamplerPresets&);

    class ShaderBinder: public RenderPassElementBinder{
    public:
        void validateAndPlan(const ValueArena&,
            const VTable&, size_t index, RenderPassElementBindPlan&
        ) override;

        static void freeze(RenderSpec&, RenderPassElementBindPlan&);
    };

    class SamplerBinder: public RenderPassElementBinder{
    private:
        using SamplerPresets = std::unordered_map<std::string, RHISamplerState>;
        SamplerPresets presets;

    public:
        SamplerBinder(SamplerPresets presets)
            :presets(presets){}

        void validateAndPlanArray(const ValueArena&,
            const VArray&, size_t index, RenderPassElementBindPlan&
        ) override;

        void validateAndPlan(const ValueArena&,
            const VTable&, size_t index, RenderPassElementBindPlan&
        ) override;

        static void freeze(RenderSpec&, RenderPassElementBindPlan&);
    };
}
