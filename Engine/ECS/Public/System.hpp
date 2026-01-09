#pragma once

#include "semantics.hpp"

namespace Crowy
{
    class EntityRegistry;

    template<typename Context>
    class System{
    public:
        CROWY_DECLARE_INTERFACE(System)

        virtual void update(EntityRegistry&, Context&) = 0;
    };
}