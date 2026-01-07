#pragma once

#include "System.hpp"
#include "UpdateContext.hpp"

namespace Crowy
{
    class RenderSystem: public System<UpdateContext>{
    public:
        void update(EntityRegistry&, UpdateContext&) override;
    };
}