#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    // An effect at its actor: the World's side of one effect of the
    // EffectSystem the World must provide, added there by its owner and named
    // here. The Extract after the actor lands or moves writes its world
    // position as the effect's origin; the size stays the effect's own. The
    // effect outlives the component.
    struct EffectComponent {
        Str effect;
    };
}
