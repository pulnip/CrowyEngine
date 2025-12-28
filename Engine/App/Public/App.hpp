#pragma once

#include <memory>
#include "error.hpp"

namespace Crowy
{
    class MainLoop;

    class App{
    public:
        // Just Singleton
        App() = delete;

        static Error setup(int argc, char* argv[]);
        static bool start();
        static void cleanup();

    private:
        static std::unique_ptr<MainLoop> mainLoop;
    };
}