#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "RenderLight.hpp"
#include "RenderMaterial.hpp"

// The render scene's rows as reflected types, for every sample that shows
// them in a panel or to the port; one registration per executable.
namespace Crowy
{
    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(LightSnapshot)
        .SetProperty("kind", &LightSnapshot::kind)
        .SetProperty("enabled", &LightSnapshot::enabled)
        .SetProperty("castShadow", &LightSnapshot::castShadow)
        .SetProperty("color", &LightSnapshot::color)
        .SetProperty("intensity", &LightSnapshot::intensity)
        .SetUIRange(0.0f, 16.0f)
        .SetProperty("position", &LightSnapshot::position)
        .SetProperty("direction", &LightSnapshot::direction)
        .SetProperty("range", &LightSnapshot::range)
        .SetUIRange(0.0f, 20.0f)
        .SetProperty("innerConeAngle", &LightSnapshot::innerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .SetProperty("outerConeAngle", &LightSnapshot::outerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .SetProperty("shadowBias", &LightSnapshot::shadowBias)
        .SetUIRange(0.0f, 0.1f)
        .SetProperty("shadowNormalBias", &LightSnapshot::shadowNormalBias)
        .SetUIRange(0.0f, 4.0f)
    CROWY_STRUCT_END(LightSnapshot)

    CROWY_STRUCT(EnvironmentSnapshot)
        .SetProperty("skyAmbient", &EnvironmentSnapshot::skyAmbient)
        .SetProperty("groundAmbient", &EnvironmentSnapshot::groundAmbient)
    CROWY_STRUCT_END(EnvironmentSnapshot)

    CROWY_STRUCT(MaterialData)
        .SetProperty("albedo", &MaterialData::albedo)
        .SetProperty("metallic", &MaterialData::metallic)
        .SetUIRange(0.0f, 1.0f)
        .SetProperty("emissive", &MaterialData::emissive)
        .SetProperty("roughness", &MaterialData::roughness)
        .SetUIRange(0.0f, 1.0f)
        .SetProperty("opacity", &MaterialData::opacity)
        .SetUIRange(0.0f, 1.0f)
        .SetProperty("custom0", &MaterialData::custom0)
        .SetProperty("custom1", &MaterialData::custom1)
        .SetProperty("uvScaleOffset", &MaterialData::uvScaleOffset)
        .SetProperty("alphaCutoff", &MaterialData::alphaCutoff)
        .SetUIRange(0.0f, 1.0f)
    CROWY_STRUCT_END(MaterialData)
    // clang-format on
}
