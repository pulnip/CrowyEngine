#pragma once

#include <filesystem>
#include <string_view>
#include "RenderSpec.hpp"

namespace Crowy
{
    RenderSpec parseRenderFromFile(const std::filesystem::path& renderFile);
    RenderSpec parseRenderFromString(std::string_view renderText);
}