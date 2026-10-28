#pragma once

#include "RenderLight.hpp"

namespace Crowy
{
    // one row of the RenderScene the World provides, landing and leaving with
    // the component at Sync; it shines along the actor's +Z
    struct LightComponent {
        LightKind kind = LightKind::Directional;

        bool enabled = true;
        bool castShadow = false;

        Vec3 color = ones();
        f32 intensity = 1.0f;

        // metres to zero; point, spot
        f32 range = 10.0f;
        // half-angles, radians
        f32 innerConeAngle = 0.0f;
        f32 outerConeAngle = 0.785f;

        // metres toward the light
        f32 shadowBias = 0.02f;
        // shadow-map texels along the normal
        f32 shadowNormalBias = 1.0f;

        // the row this component owns, from the Sync it lands at
        LightHandle row;
    };
}
