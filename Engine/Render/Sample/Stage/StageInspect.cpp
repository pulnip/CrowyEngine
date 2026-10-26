#include "ClassRegistry.hpp"
#include "StageDocument.hpp"

// The scene file's rows as the inspector edits them, in meters and degrees;
// names stay out so the selection keeps naming its row.
namespace Crowy
{
    // clang-format off
    CROWY_STRUCT(StageInstance)
        .AddProperty("position", &StageInstance::position)
        .AddProperty("yaw", &StageInstance::yaw)
        .AddProperty("scale", &StageInstance::scale)
    CROWY_STRUCT_END(StageInstance)

    CROWY_STRUCT(StageQuad)
        .AddProperty("position", &StageQuad::position)
        .AddProperty("yaw", &StageQuad::yaw)
        .AddProperty("width", &StageQuad::width)
        .AddProperty("height", &StageQuad::height)
        .AddProperty("uv0", &StageQuad::uv0)
        .AddProperty("uv1", &StageQuad::uv1)
    CROWY_STRUCT_END(StageQuad)

    // the intensity before the key's group scale, the cone in degrees
    CROWY_STRUCT(StageLight)
        .AddProperty("position", &StageLight::position)
        .AddProperty("direction", &StageLight::direction)
        .AddProperty("color", &StageLight::color)
        .AddProperty("intensity", &StageLight::intensity)
        .AddProperty("range", &StageLight::range)
        .AddProperty("innerAngle", &StageLight::innerAngle)
        .SetUIRange(0.0f, 90.0f)
        .AddProperty("outerAngle", &StageLight::outerAngle)
        .SetUIRange(0.0f, 90.0f)
    CROWY_STRUCT_END(StageLight)
    // clang-format on
}
