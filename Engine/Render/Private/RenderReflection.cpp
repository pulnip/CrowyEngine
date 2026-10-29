#include "ClassRegistry.hpp"
#include "RenderLight.hpp"
#include "RenderMaterial.hpp"

namespace Crowy
{
    // clang-format off
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
