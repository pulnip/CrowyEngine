#pragma once

#include "Primitives.hpp"
#include "SampleScene.hpp"

namespace Crowy
{
    // A cubemap skybox over a textured ground plane.
    //
    // The floor lies in the XZ plane at y = 0, facing up, and carries the
    // Blender UV grid, which makes texture orientation and filtering obvious
    // at a glance. By default the grid maps once across the whole plane, so
    // growing halfSize grows the pattern with it rather than repeating it.
    // Raising uvScale tiles the grid instead, which is the quickest way to
    // watch filtering fall apart at grazing angles.
    //
    // The scene carries no lights: the sky is the light source, and a sample
    // is free to treat it as an environment probe or to ignore it entirely.
    SceneData MakeSkyboxFloor(f32 halfSize = 10.0f, f32 uvScale = 1.0f);
}
