#pragma once

#include "semantics.hpp"

namespace Crowy
{
    class EntityRegistry;

    template<typename Context>
    class System{
    public:
        CROWY_DECLARE_INTERFACE(System)

        virtual void start(EntityRegistry&){}
        virtual void update(EntityRegistry&, Context&) = 0;
        virtual void finish(EntityRegistry&){}
    };
}