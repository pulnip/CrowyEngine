#pragma once

#include <optional>
#include <string>
#include <utility>
#include "MeshData.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    enum class SchemeKind{
        File     = 0,
        Embedded = 1,
        Unknown  = 2
    };

    // "scheme:path" -> {scheme, path}
    std::pair<SchemeKind, std::string> splitSchemeAndPath(const std::string& uri);

    // 3D Model Files to MeshData
    std::optional<MeshData> importMesh(const std::string& filePath, RHICapabilities);

    // Load embedded primitive mesh (cube, sphere, plane, etc.)
    std::optional<MeshData> loadEmbeddedMesh(const std::string& name);
}
