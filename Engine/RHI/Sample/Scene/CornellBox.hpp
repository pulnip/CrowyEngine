#pragma once

#include "Primitives.hpp"
#include "SampleScene.hpp"

namespace Crowy
{
    // The Cornell box, with three spheres on the floor instead of the two
    // boxes of the original.
    //
    // The room is a cube spanning [-halfSize, halfSize] on every axis, open
    // toward -Z so a camera sitting on the -Z side looks straight into it.
    // Left wall (-X) is red and right wall (+X) is green, which is the classic
    // arrangement as seen from that camera. Every wall faces inward, so the
    // room renders with the same clockwise-front rule as any other mesh and
    // needs no cull-mode change.
    //
    // The spheres vary metallic and roughness so that a shading sample has
    // something to tell apart, and the ceiling carries one area light.
    SceneData MakeCornellBox(f32 halfSize = 1.0f);
}
