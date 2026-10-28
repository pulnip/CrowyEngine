#pragma once

#include <functional>

#include "Actor.hpp"
#include "System.hpp"

namespace Crowy
{
    // one actor's own behaviour, for what is not worth a system: run once a
    // frame in Update, after the ticks, in row order
    struct ScriptComponent {
        std::function<void(World&, ActorId, const FrameTime&)> run;
    };
}
