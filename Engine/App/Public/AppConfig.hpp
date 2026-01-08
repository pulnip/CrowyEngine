#pragma once

#include <filesystem>
#include "OS.hpp"

namespace Crowy
{
    struct AppConfig{
        WindowConfig window;
        std::filesystem::path sceneFile;
        std::filesystem::path renderFile;
    };

    AppConfig parseCommandLine(int argc, char* argv[]);
}