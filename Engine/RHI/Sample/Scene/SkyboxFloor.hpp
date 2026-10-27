#pragma once

#include "Primitives.hpp"
#include "SampleScene.hpp"

namespace Crowy
{
    // The floor lies at y = 0 facing up, the UV grid mapped once across it
    // (uvScale tiles it). No lights: the sky is the only light source.
    SceneData MakeSkyboxFloor(f32 halfSize = 10.0f, f32 uvScale = 1.0f);
}
