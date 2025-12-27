#pragma once

#include "BinderRegistry.hpp"
#include "RenderSpec.hpp"

namespace Crowy
{
    struct PlannedShader{
        ShaderSpec spec;
        size_t index = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };

    struct RenderPassElementBindPlan{
        std::vector<PlannedShader> shaders;
        std::vector<BindError> errors;
    };

    using RenderPassElementBinder = Binder<RenderPassElementBindPlan>;
    using RenderPassBinderRegistry = BinderRegistry<RenderPassElementBindPlan>;
    RenderPassBinderRegistry makeRenderPassBinderRegistry();

    class ShaderBinder: public RenderPassElementBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t index, RenderPassElementBindPlan& plan
        ) override;

        static void freeze(RenderSpec&, RenderPassElementBindPlan&);
    };
}
