#include "ClassRegistry.hpp"
#include "RenderLight.hpp"
#include "RenderMaterial.hpp"

// The render scene's rows as reflected types, for every sample that shows
// them in a panel or to the port.
namespace Crowy
{
    // clang-format off
    CROWY_STRUCT(LightSnapshot)
        .AddProperty("kind", &LightSnapshot::kind)
        .AddProperty("enabled", &LightSnapshot::enabled)
        .AddProperty("castShadow", &LightSnapshot::castShadow)
        .AddProperty("color", &LightSnapshot::color)
        .AddProperty("intensity", &LightSnapshot::intensity)
        .SetUIRange(0.0f, 16.0f)
        .AddProperty("position", &LightSnapshot::position)
        .AddProperty("direction", &LightSnapshot::direction)
        .AddProperty("range", &LightSnapshot::range)
        .SetUIRange(0.0f, 20.0f)
        .AddProperty("innerConeAngle", &LightSnapshot::innerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .AddProperty("outerConeAngle", &LightSnapshot::outerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .AddProperty("shadowBias", &LightSnapshot::shadowBias)
        .SetUIRange(0.0f, 0.1f)
        .AddProperty("shadowNormalBias", &LightSnapshot::shadowNormalBias)
        .SetUIRange(0.0f, 4.0f)
    CROWY_STRUCT_END(LightSnapshot)

    CROWY_STRUCT(EnvironmentSnapshot)
        .AddProperty("skyAmbient", &EnvironmentSnapshot::skyAmbient)
        .AddProperty("groundAmbient", &EnvironmentSnapshot::groundAmbient)
    CROWY_STRUCT_END(EnvironmentSnapshot)

    CROWY_STRUCT(MaterialData)
        .AddProperty("albedo", &MaterialData::albedo)
        .AddProperty("metallic", &MaterialData::metallic)
        .SetUIRange(0.0f, 1.0f)
        .AddProperty("emissive", &MaterialData::emissive)
        .AddProperty("roughness", &MaterialData::roughness)
        .SetUIRange(0.0f, 1.0f)
        .AddProperty("opacity", &MaterialData::opacity)
        .SetUIRange(0.0f, 1.0f)
        .AddProperty("custom0", &MaterialData::custom0)
        .AddProperty("custom1", &MaterialData::custom1)
        .AddProperty("uvScaleOffset", &MaterialData::uvScaleOffset)
        .AddProperty("alphaCutoff", &MaterialData::alphaCutoff)
        .SetUIRange(0.0f, 1.0f)
    CROWY_STRUCT_END(MaterialData)
    // clang-format on
}
