#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    // names an effect its owner added to the EffectSystem the World provides;
    // the actor's world position becomes its origin, and it outlives this
    struct EffectComponent {
        Str effect;
    };
}
