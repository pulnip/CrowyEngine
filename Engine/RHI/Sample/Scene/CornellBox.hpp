#pragma once

#include "Primitives.hpp"
#include "SampleScene.hpp"

namespace Crowy
{
    // Open toward -Z, red at -X and green at +X as seen from a camera on -Z.
    // Every wall faces inward, so the room needs no cull-mode change.
    SceneData MakeCornellBox(f32 halfSize = 1.0f);
}
