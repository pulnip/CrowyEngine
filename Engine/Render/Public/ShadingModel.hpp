#pragma once

#include "Primitives.hpp"

// The engine's shading models: Slang modules a material links into its
// colour passes, each exporting MaterialShading for ForwardShading.slang.
namespace Crowy
{
    struct ToonLanes;

    // glTF metallic-roughness, the default
    inline constexpr CStr PBRShadingModule = "Engine/Shader/PBR.slang";
    // cel shading: a terminator band, a hard highlight, a rim
    inline constexpr CStr ToonShadingModule = "Engine/Shader/Toon.slang";
    // the surface's own colour, no light
    inline constexpr CStr UnlitShadingModule = "Engine/Shader/Unlit.slang";

    // Toon.slang's reading of the lanes, custom0 then custom1
    struct ToonLanes {
        // the attenuation a light must reach to count as lighting; 0 lights
        // what the shadow darkens
        f32 shadowThreshold = 0.5f;
        // the highlight disc's size as 1 - cos of its radius; 0 turns it off
        f32 highlightSize = 0.05f;
        f32 rimStrength = 0.0f;
        // how far in from the silhouette the rim reaches, in 1 - N.V
        f32 rimWidth = 0.3f;
        // the unlit side, multiplied into the albedo; black blackens it
        Vec3 shadeTint{0.30f, 0.28f, 0.42f};
    };

    // after the struct: correctness outranks declaration order
    inline constexpr Vec4 toonCustom0(const ToonLanes& lanes) {
        return {
            lanes.shadowThreshold,
            lanes.highlightSize,
            lanes.rimStrength,
            lanes.rimWidth
        };
    }

    inline constexpr Vec4 toonCustom1(const ToonLanes& lanes) {
        return {lanes.shadeTint.x, lanes.shadeTint.y, lanes.shadeTint.z, 0.0f};
    }
}
