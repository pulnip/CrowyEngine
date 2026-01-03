#pragma once

#include <string_view>
#include "InputSpec.hpp"

namespace Crowy
{
    InputSpec parseInputFromFile(std::string_view sceneFile);
    InputSpec parseInputFromString(std::string_view sceneText);
}