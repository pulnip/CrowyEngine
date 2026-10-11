#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // What extraction writes for the camera a view is drawn through; the
    // projection waits for the aspect of the target it is drawn into.
    struct CameraSnapshot {
        Mat4 view = unitMat();
        Vec3 position = zeros();

        f32 fovY = 1.0471976f;
        f32 nearZ = 0.1f;
        f32 farZ = 1000.0f;
    };
}
