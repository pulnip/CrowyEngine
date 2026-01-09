#pragma once

#include <cstdint>
#include <string>
#include "math.hpp"

namespace Crowy
{
    enum class CameraType: uint8_t{
        UNKNOWN     = uint8_t(-1),
        MainCamera  =  0,
        SubCamera   =  1,
    };

    enum class Projection: uint8_t{
        UNKNOWN     = uint8_t(-1),
        PERSPECTIVE =  0,
        ORTHOGRAPHIC=  1,
    };

    struct Ray{
        Vec3 point;
        Vec3 dir;
    };

    struct alignas(16) Line{
        Vec4 from, to;
        Vec4 color;
    };
    struct Sphere{
        Vec3 point;
        float radius;
        Vec4 color;
    };

    CameraType toCameraType(const std::string& text);
    Projection toProjection(const std::string& text);
}