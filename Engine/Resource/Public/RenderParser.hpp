#pragma once

#include <string_view>
#include "RenderSpec.hpp"

namespace Crowy
{
    RenderSpec parseRenderFromFile(std::string_view sceneFile);
    RenderSpec parseRenderFromString(std::string_view sceneText);
}