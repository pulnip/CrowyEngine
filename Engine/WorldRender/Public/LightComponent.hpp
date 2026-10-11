#pragma once

#include "RenderLight.hpp"

namespace Crowy
{
    // A light at its actor: the World's side of one RenderScene light row,
    // which lands and leaves with the component at Sync. The row's position
    // is the actor's, and it shines along the actor's +Z. Rows keep the
    // order the lights landed in, which picks the shadow caster. The World
    // must provide the RenderScene.
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
