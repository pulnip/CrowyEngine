#pragma once

#include <concepts>
#include <exception>
#include <filesystem>
#include <print>
#include <span>

#include "MainLoop.hpp"
#include "OS.hpp"
#include "ProjectRoot.hpp"
#include "RHIDevice.hpp"
#include "RuntimeConfig.hpp"

namespace Crowy
{
    // an app that opens the scene file named on its command line says so
    template<typename T>
    concept SceneOpener = requires { requires T::OpensScene; };

    template<std::derived_from<MainLoop> T>
    int Main(const RuntimeConfig& runtimeConfig) {
        try {
            if(runtimeConfig.project)
                setProjectRoot(*runtimeConfig.project);

            auto device = createDevice();

            OS os(runtimeConfig, *device);
            T app;

            os.Run(app);
        }
        catch(const std::exception& e) {
            std::println("Exception: {}", e.what());

            return 1;
        }
        catch(...) {
            std::println("Unhandled Exception");

            return 1;
        }

        return 0;
    }

    template<std::derived_from<MainLoop> T>
    int Main(int argc, char** argv, const WindowConfig& windowConfig) {
        RuntimeConfig runtimeConfig{.window = windowConfig};
        if(argc > 0 && runtimeConfig.window.title == WindowConfig{}.title)
            runtimeConfig.window.title =
                std::filesystem::path(argv[0]).stem().string();

        try {
            const auto args =
                std::span<char* const>(argv, static_cast<usize>(argc));
            applyCommandLine(
                runtimeConfig,
                args.empty() ? args : args.subspan(1),
                SceneOpener<T> ? SceneArgument::Required : SceneArgument::None
            );
        }
        catch(const std::exception& e) {
            std::println("{}", e.what());

            return 1;
        }

        return Main<T>(runtimeConfig);
    }
}
