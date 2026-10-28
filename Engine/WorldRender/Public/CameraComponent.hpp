#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    // at its actor, looking along its +Z; one per World, extracted into the
    // CameraSnapshot the World provides
    struct CameraComponent {
        f32 fovY = 1.0471976f;
        f32 nearZ = 0.1f;
        f32 farZ = 1000.0f;
    };
}
