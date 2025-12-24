#pragma once

#include <optional>
#include <string>
#include <utility>
#include "ModelData.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    // uri -> ModelData
    // uri ex.: embedded:cube, file:asset/model.fbx
    std::optional<ModelData> importModel(const std::string& uri, RHICapabilities);
}
