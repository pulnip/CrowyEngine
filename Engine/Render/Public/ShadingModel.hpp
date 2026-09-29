#pragma once

#include "Primitives.hpp"

// The engine's shading models: Slang modules a material links into its
// colour passes, each exporting MaterialShading for ForwardShading.slang.
namespace Crowy
{
    // glTF metallic-roughness, the default
    inline constexpr CStr PBRShadingModule = "Engine/Shader/PBR.slang";
}
