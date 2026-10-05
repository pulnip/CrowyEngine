#pragma once

#include <cmath>

#include "Primitives.hpp"

namespace Crowy
{
    // IEC 61966-2-1 decoding, the CPU twin of Engine/Shader/SRGB.slang's
    // srgb2linear, edge included
    inline f32 srgbToLinear(f32 encoded) {
        return encoded < 0.04045f
            ? encoded / 12.92f
            : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
    }

    inline Vec3 srgbToLinear(Vec3 encoded) {
        return Vec3{
            srgbToLinear(encoded.x),
            srgbToLinear(encoded.y),
            srgbToLinear(encoded.z)
        };
    }
}
