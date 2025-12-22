#pragma once

#include <optional>
#include <string>
#include "MeshData.hpp"

namespace Crowy
{
    struct RHICapabilities;

    // 3D Model Files to MeshData
    std::optional<MeshData> importMesh(const std::string& filePath, RHICapabilities);

    // Load embedded primitive mesh (cube, sphere, plane, etc.)
    std::optional<MeshData> loadEmbeddedMesh(const std::string& name);
}
