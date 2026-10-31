#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // the environment of the RenderScene the World provides, one per World;
    // its sun is a directional LightComponent on the same actor. Colors are
    // linear.
    struct EnvironmentComponent {
        Vec3 skyAmbient = zeros();
        Vec3 groundAmbient = zeros();
        // what the frame clears to
        Vec3 skyColor = zeros();
    };
}
