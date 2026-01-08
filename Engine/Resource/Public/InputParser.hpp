#pragma once

#include <filesystem>
#include <string_view>
#include "InputSpec.hpp"

namespace Crowy
{
    InputSpec parseInputFromFile(const std::filesystem::path& sceneFile);
    InputSpec parseInputFromString(std::string_view sceneText);
}