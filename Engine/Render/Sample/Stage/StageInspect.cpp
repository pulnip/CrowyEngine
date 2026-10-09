#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "StageDocument.hpp"

// The scene file's rows as the inspector edits them, in meters and degrees;
// names stay out so the selection keeps naming its row.
namespace Crowy
{
    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(StageInstance)
        .SetProperty("position", &StageInstance::position)
        .SetProperty("yaw", &StageInstance::yaw)
        .SetProperty("scale", &StageInstance::scale)
    CROWY_STRUCT_END(StageInstance)

    CROWY_STRUCT(StageQuad)
        .SetProperty("position", &StageQuad::position)
        .SetProperty("yaw", &StageQuad::yaw)
        .SetProperty("width", &StageQuad::width)
        .SetProperty("height", &StageQuad::height)
        .SetProperty("uv0", &StageQuad::uv0)
        .SetProperty("uv1", &StageQuad::uv1)
    CROWY_STRUCT_END(StageQuad)

    // the intensity before the key's group scale, the cone in degrees
    CROWY_STRUCT(StageLight)
        .SetProperty("position", &StageLight::position)
        .SetProperty("direction", &StageLight::direction)
        .SetProperty("color", &StageLight::color)
        .SetProperty("intensity", &StageLight::intensity)
        .SetProperty("range", &StageLight::range)
        .SetProperty("innerAngle", &StageLight::innerAngle)
        .SetUIRange(0.0f, 90.0f)
        .SetProperty("outerAngle", &StageLight::outerAngle)
        .SetUIRange(0.0f, 90.0f)
    CROWY_STRUCT_END(StageLight)
    // clang-format on
}
