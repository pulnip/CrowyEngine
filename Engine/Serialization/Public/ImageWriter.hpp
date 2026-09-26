#pragma once

#include <filesystem>

#include "ImageView.hpp"

namespace Crowy
{
    // RGB unless alpha: the back buffer's alpha is arbitrary and never
    // compared. Throws std::runtime_error when the file cannot be written.
    void writePng(
        const std::filesystem::path& path,
        Rgba8View image,
        bool alpha = false
    );
}
