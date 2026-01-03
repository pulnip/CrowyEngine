#pragma once

#include "BinderRegistry.hpp"
#include "InputSpec.hpp"

namespace Crowy
{
    struct PlannedBindings{
        InputBindings spec;
        size_t index = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };

    struct InputElementBindPlan{
        std::vector<PlannedBindings> allBindings;
        std::vector<BindError> errors;
    };

    using InputElementBinder = Binder<InputElementBindPlan>;
    using InputBinderRegistry = BinderRegistry<InputElementBindPlan>;
    InputBinderRegistry makeInputBinderRegistry();

    class BindingsBinder: public InputElementBinder{
    public:
        void validateAndPlan(const ValueArena& arena,
            const VTable& src, size_t index, InputElementBindPlan& plan
        ) override;
        void validateAndPlanArray(const ValueArena& arena,
            const VArray& src, size_t index, InputElementBindPlan& plan
        ) override;

        static void freeze(InputSpec&, InputElementBindPlan&);
    };
}
