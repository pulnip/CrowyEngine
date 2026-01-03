#include "InputBinder.hpp"

namespace Crowy
{
    void BindingsBinder::validateAndPlan(const ValueArena& arena,
        const VTable& src, size_t index, InputElementBindPlan& plan
    ){
        // TODO
        throw std::runtime_error("Not Implemented");
    }

    void BindingsBinder::freeze(InputSpec& spec, InputElementBindPlan& plan){
        // TODO
        throw std::runtime_error("Not Implemented");
    }

    InputBinderRegistry makeInputBinderRegistry(){
        InputBinderRegistry reg;
        reg.emplace("bindings", std::make_unique<BindingsBinder>());

        return reg;
    }
}