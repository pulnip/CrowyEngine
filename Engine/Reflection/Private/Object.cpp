#include "Object.hpp"

namespace Crowy
{
    auto Object::_CrowyReflectImpl(){
        return ::Crowy::Reflect<Object>()
            .SetName("Object")
            .Build();
    }
    namespace{
        const auto IsObjectRegistered = Object::_CrowyReflectImpl();
    }
}
