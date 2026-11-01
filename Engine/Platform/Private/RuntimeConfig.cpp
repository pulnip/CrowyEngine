#include "RuntimeConfig.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

#include "DOM.hpp"
#include "DomTraits.hpp"
#include "JsonLoader.hpp"
#include "PathUtil.hpp"
#include "RHIFrameStats.hpp"
#include "RHISwapchain.hpp"
#include "RangeUtil.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    WindowConfig DomTraits<WindowConfig>::from(const DOM::Value& root) {
        auto title =
            root.get<Str>("runtime.window.title").value_or("DefaultWindow");
        auto width = root.get<u32>("runtime.window.width").value_or(800);
        auto height = root.get<u32>("runtime.window.height").value_or(600);

        auto fullscreen =
            root.get<bool>("runtime.window.fullscreen").value_or(false);
        auto resizable =
            root.get<bool>("runtime.window.resizable").value_or(false);
        auto borderless =
            root.get<bool>("runtime.window.borderless").value_or(false);
        auto always_on_top =
            root.get<bool>("runtime.window.always_on_top").value_or(false);
        auto vsync = root.get<bool>("runtime.window.vsync").value_or(true);

        return WindowConfig{
            .title = title,
            .width = width,
            .height = height,

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
    ) {
        if(metadata.type != "app") {
            throw std::runtime_error(
                std::format(
                    "expected an \"app\" document, got \"{}\"",
                    metadata.type
                )
            );
        }

        auto name = root.get<Str>("runtime.app_name").value_or("AnonymousApp");
        auto version = root.get<Str>("runtime.app_version").value_or("v0.0.1");
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

    void applyCommandLine(
        RuntimeConfig& config,
        std::span<char* const> args,
        SceneArgument sceneArgument
    ) {
        const auto takesScene = sceneArgument == SceneArgument::Required;
        const auto usage = std::format(
            "usage: <app>{} [--config <path>] [--hold] "
            "[--capture-frame <frame> <path>]... "
            "[--exit-after-frame <frame>] "
            "[--port-input] [--project <folder>]",
            takesScene ? " <scene>" : ""
        );

        const auto valueOf = [&](usize& i, StrView flag) {
            if(i + 1 == args.size()) {
                throw std::runtime_error(
                    std::format("'{}' needs a value\n{}", flag, usage)
                );
            }
            return args[++i];
        };
        const auto frameOf = [&](usize& i, StrView flag) {
            const StrView text = valueOf(i, flag);
            const auto frame = parseNumber<u64>(text);
            if(!frame || *frame == 0) {
                throw std::runtime_error(
                    std::format(
                        "'{}' needs a frame from 1, not '{}'\n{}",
                        flag,
                        text,
                        usage
                    )
                );
            }
            return *frame;
        };

        for(usize i = 0; i < args.size(); ++i) {
            const StrView arg = args[i];
            if(arg == "--hold") {
                config.hold = true;
                continue;
            }
            if(arg == "--capture-frame") {
                const auto frame = frameOf(i, arg);
                config.captures.push_back(
                    FrameCaptureConfig{.frame = frame, .path = valueOf(i, arg)}
                );
                continue;
            }
            if(arg == "--port-input") {
                config.portInput = true;
                continue;
            }
            if(arg == "--exit-after-frame") {
                config.exitAfterFrame = frameOf(i, arg);
                continue;
            }
            if(arg == "--project") {
                config.project = toPath(valueOf(i, arg));
                continue;
            }
            if(takesScene && !arg.starts_with("--")) {
                if(config.scene) {
                    throw std::runtime_error(
                        std::format(
                            "a second scene '{}' after '{}'\n{}",
                            arg,
                            config.scene->string(),
                            usage
                        )
                    );
                }
                config.scene = toPath(arg);
                continue;
            }
            if(arg != "--config") {
                throw std::runtime_error(
                    std::format("unknown argument '{}'\n{}", arg, usage)
                );
            }

            const auto loaded =
                loadJsonFile<RuntimeConfig>(toPath(valueOf(i, arg)));
            config.benchmark = loaded.benchmark;
            config.window.vsync = loaded.window.vsync;
        }

        if(takesScene && !config.scene) {
            throw std::runtime_error(
                std::format("a scene file is needed\n{}", usage)
            );
        }

        // what the swapchain would refuse, refused before the window opens
        const auto& captures = config.captures;
        if(captures.size() > MaxFrameDumps) {
            throw std::runtime_error(
                std::format(
                    "more than {} '--capture-frame'\n{}",
                    MaxFrameDumps,
                    usage
                )
            );
        }
        const auto repeat = std::min(
            firstRepeat(captures, &FrameCaptureConfig::frame),
            firstRepeat(captures, &FrameCaptureConfig::path)
        );
        if(repeat != captures.end()) {
            throw std::runtime_error(
                std::format(
                    "two '--capture-frame' share frame {} or '{}'\n{}",
                    repeat->frame,
                    repeat->path,
                    usage
                )
            );
        }

        if(const auto exit = config.exitAfterFrame) {
            for(const auto& capture: captures) {
                if(capture.frame > *exit) {
                    throw std::runtime_error(
                        std::format(
                            "the capture at frame {} comes after the exit at "
                            "frame {}\n{}",
                            capture.frame,
                            *exit,
                            usage
                        )
                    );
                }
            }
        }

        // fail now rather than run until the bench script's timeout
        if(config.benchmark.enabled && !CROWY_FRAME_STATS) {
            throw std::runtime_error(
                "benchmark.enabled needs a Debug or CROWY_BENCHMARK build"
            );
        }
    }
}
