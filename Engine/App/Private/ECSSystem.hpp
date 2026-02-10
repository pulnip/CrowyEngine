#pragma once

#include "System.hpp"
#include "Context.hpp"

namespace Crowy
{
    class RenderSystem: public System<UpdateContext>{
    public:
        void update(EntityRegistry&, UpdateContext&) override;
    };
}