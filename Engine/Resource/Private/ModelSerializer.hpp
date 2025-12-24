#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "ModelData.hpp"

namespace Crowy
{
    std::vector<uint8_t> serializeMesh(const ModelData&);
    std::optional<ModelData> deserializeMesh(std::span<const uint8_t>);
}