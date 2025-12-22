#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "MeshData.hpp"

namespace Crowy
{
    std::vector<uint8_t> serializeMesh(const MeshData& mesh);
    std::optional<MeshData> deserializeMesh(std::span<const uint8_t> data);
}