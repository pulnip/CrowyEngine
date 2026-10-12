#pragma once

#include <functional>

#include "Actor.hpp"
#include "System.hpp"

namespace Crowy
{
    // One actor's own behaviour, for what is not worth a system: a function
    // run once a frame in Update, after the frame's ticks, in the order the
    // scripts landed. A destroyed actor's script never runs again.
    struct ScriptComponent {
        std::function<void(World&, ActorId, const FrameTime&)> run;
    };
}
