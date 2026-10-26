#pragma once

#include <filesystem>

#include "AssetRef.hpp"

namespace Crowy
{
    struct AssetRoots {
        std::filesystem::path engine;
        std::filesystem::path content;
    };

    std::filesystem::path resolveAssetPath(
        const AssetRoots& roots,
        const AssetRef& ref
    );
}
