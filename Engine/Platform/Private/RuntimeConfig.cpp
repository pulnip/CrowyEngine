#include "RuntimeConfig.hpp"
#include <format>
#include <stdexcept>
#include "DOM.hpp"
#include "DomTraits.hpp"
#include "JsonLoader.hpp"
#include "RHIFrameStats.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    WindowConfig DomTraits<WindowConfig>::from(
        const DOM::Value& root
    ){
        auto title = root.get<Str>("runtime.window.title")
            .value_or("DefaultWindow");
        auto width = root.get<u32>("runtime.window.width")
            .value_or(800);
        auto height = root.get<u32>("runtime.window.height")
            .value_or(600);

        // Window Options
        auto fullscreen = root.get<bool>("runtime.window.fullscreen")
            .value_or(false);
        auto resizable = root.get<bool>("runtime.window.resizable")
            .value_or(false);
        auto borderless = root.get<bool>("runtime.window.borderless")
            .value_or(false);
        auto always_on_top = root.get<bool>("runtime.window.always_on_top")
            .value_or(false);
        auto vsync = root.get<bool>("runtime.window.vsync")
            .value_or(true);

        return WindowConfig{
            .title = title,
            .width = width, .height = height,

            .fullscreen = fullscreen,
            .resizable = resizable,
            .borderless = borderless,
            .always_on_top = always_on_top,
            .vsync = vsync
        };
    }

    RuntimeConfig DomTraits<RuntimeConfig>::from(
        const DOM::Value& root,
        const DocMetadata& metadata
    ){
        if(metadata.type != "app"){
            throw std::runtime_error(std::format(
                "expected an \"app\" document, got \"{}\"", metadata.type
            ));
        }

        auto name = root.get<Str>("runtime.app_name")
            .value_or("AnonymousApp");
        auto version = root.get<Str>("runtime.app_version")
            .value_or("v0.0.1");
        auto identifier = root.get<Str>("runtime.app_identifier")
            .value_or("AnonymousIdentifier");

        const BenchmarkConfig defaults;
        auto benchmark = BenchmarkConfig{
            .enabled = root.get<bool>("runtime.benchmark.enabled")
                .value_or(defaults.enabled),
            .warmupFrames = root.get<u32>("runtime.benchmark.warmup_frames")
                .value_or(defaults.warmupFrames),
            .measureFrames = root.get<u32>("runtime.benchmark.measure_frames")
                .value_or(defaults.measureFrames),
            .reportPath = root.get<Str>("runtime.benchmark.report_path")
                .value_or(defaults.reportPath),
            .framePath = root.get<Str>("runtime.benchmark.frame_path")
                .value_or(defaults.framePath)
        };

        return RuntimeConfig{
            .name = name,
            .version = version,
            .identifier = identifier,
            .window = DomTraits<WindowConfig>::from(root),
            .benchmark = benchmark
        };
    }

    void applyCommandLine(RuntimeConfig& config, std::span<char* const> args){
        constexpr auto Usage = "usage: <app> [--config <path>]";

        for(usize i = 0; i < args.size(); ++i){
            const StrView arg = args[i];
            if(arg != "--config"){
                throw std::runtime_error(std::format(
                    "unknown argument '{}'\n{}", arg, Usage
                ));
            }
            if(i + 1 == args.size()){
                throw std::runtime_error(std::format(
                    "'--config' needs a path\n{}", Usage
                ));
            }

            const auto loaded = loadJsonFile<RuntimeConfig>(toPath(args[++i]));
            config.benchmark = loaded.benchmark;
            config.window.vsync = loaded.window.vsync;
        }

        // fail now rather than run until the bench script's timeout
        if(config.benchmark.enabled && !CROWY_FRAME_STATS){
            throw std::runtime_error(
                "benchmark.enabled needs a Debug or CROWY_BENCHMARK build"
            );
        }
    }
}
