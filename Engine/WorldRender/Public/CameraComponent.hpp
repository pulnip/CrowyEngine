#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    // The camera a World is seen through, at its actor and looking along the
    // actor's +Z. One per World until a view can choose among several; a
    // second one stops the process. The World must provide the
    // CameraSnapshot it is extracted into.
    struct CameraComponent {
        f32 fovY = 1.0471976f;
        f32 nearZ = 0.1f;
        f32 farZ = 1000.0f;
    };
}
