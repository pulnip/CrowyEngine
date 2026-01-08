#pragma once

#include <filesystem>
#include <string_view>
#include "SceneSpec.hpp"

namespace Crowy
{
    SceneSpec parseSceneFromFile(const std::filesystem::path& sceneFile);
    SceneSpec parseSceneFromString(std::string_view sceneText);
}