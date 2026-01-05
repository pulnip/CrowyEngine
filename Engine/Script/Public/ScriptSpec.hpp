#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Crowy
{
    struct ScriptModuleSpec{
        std::string name;
        std::vector<std::filesystem::path> files;
    };

    struct ScriptSpec{
        std::vector<ScriptModuleSpec> modules;
    };

    struct ScriptInstanceSpec{
        std::vector<std::string> monoScripts;
    };
}