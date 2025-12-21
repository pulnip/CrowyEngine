#pragma once

#include <string_view>
#include "SceneSpec.hpp"

namespace Crowy
{
    SceneSpec parseSceneFromFile(std::string_view sceneFile);
    SceneSpec parseSceneFromString(std::string_view sceneText);
}