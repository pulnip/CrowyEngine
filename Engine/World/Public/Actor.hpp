#pragma once

#include "GenericHandle.hpp"

namespace Crowy
{
    struct ActorTag;

    // an actor's slot and generation; a destroyed actor's handle stays
    // invalid after its slot is reused
    using ActorId = GenericHandle<ActorTag>;
}
