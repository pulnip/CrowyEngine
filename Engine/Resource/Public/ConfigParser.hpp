#pragma once

#include <filesystem>
#include <string_view>
#include "InputSpec.hpp"
#include "RenderSpec.hpp"
#include "SceneSpec.hpp"
#include "ScriptSpec.hpp"

namespace Crowy
{
    InputSpec parseInputFromFile(const std::filesystem::path& inputFile);
    InputSpec parseInputFromString(std::string_view inputText);

    RenderSpec parseRenderFromFile(const std::filesystem::path& renderFile);
    RenderSpec parseRenderFromString(std::string_view renderText);

    SceneSpec parseSceneFromFile(const std::filesystem::path& sceneFile);
    SceneSpec parseSceneFromString(std::string_view sceneText);

    ScriptSpec parseScriptFromFile(const std::filesystem::path& scriptFile);
    ScriptSpec parseScriptFromString(std::string_view scriptText);
}