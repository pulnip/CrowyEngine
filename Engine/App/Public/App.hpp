#pragma once

#include <memory>
#include "error.hpp"

namespace Crowy
{
    class MainLoop;
    struct AppConfig;

    class App{
    public:
        // Just Singleton
        App() = delete;

        static Error setup(const AppConfig&);
        static void cleanup();

    private:
        static std::unique_ptr<MainLoop> mainLoop;
    };
}