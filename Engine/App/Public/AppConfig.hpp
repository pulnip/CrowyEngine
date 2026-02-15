#pragma once

#include <filesystem>
#include "OS.hpp"

namespace Crowy
{
    struct AppConfig{
        WindowConfig window;
        std::filesystem::path renderFile;
        std::filesystem::path inputFile;
        std::filesystem::path scriptFile;
        std::filesystem::path sceneFile;
    };

    AppConfig parseCommandLine(int argc, char* argv[]);
}