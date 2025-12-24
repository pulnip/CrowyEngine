#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "ModelData.hpp"

namespace Crowy
{
    std::vector<uint8_t> serializeModel(const ModelData&);
    std::optional<ModelData> deserializeModel(std::span<const uint8_t>);
}