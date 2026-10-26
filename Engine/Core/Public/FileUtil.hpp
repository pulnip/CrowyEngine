#pragma once

#include <filesystem>
#include <vector>

#include "Primitives.hpp"

namespace Crowy
{
    Str readFileAsString(const std::filesystem::path&);
    std::vector<u8> readFileAsBinary(const std::filesystem::path&);
}
