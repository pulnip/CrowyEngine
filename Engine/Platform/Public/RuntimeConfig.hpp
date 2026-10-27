#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "DOM.hpp"
#include "DomTraits.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct WindowConfig {
        Str title = "DefaultWindow";
        u32 width = 800, height = 600;
        RHIPixelFormat format = RHIPixelFormat::RGBA8_UNORM_SRGB;

        bool fullscreen = false;
        bool resizable = false;
        bool borderless = false;
        bool always_on_top = false;
        // false: no click or Alt+Tab focuses it, as a --port-input run's
        bool focusable = true;

        // a timed run wants Present(0, 0): with vsync on, every frame time
        // is really the refresh rate talking
        bool vsync = true;
    };

    template<>
    struct DomTraits<WindowConfig> {
        static WindowConfig from(const DOM::Value&);
    };

    // Set by the sample itself, so its source says what the run is for.
    // Only has teeth where CROWY_FRAME_STATS is on: Debug, or CROWY_BENCHMARK.
    struct BenchmarkConfig {
        bool enabled = false;
        // dropped, not measured: pipeline creation, shader caches, first-touch
        // page faults and the GPU settling on its clocks, about two seconds
        u32 warmupFrames = 400;
        // measured, then the loop quits on its own
        u32 measureFrames = 1000;

        // both optional; empty means don't write that one
        Str reportPath; // percentile summary, for reading
        Str framePath;  // one row per frame, for regressing against draw counts
    };

    // one frame written to a BMP, as the command port's capture_frame writes
    // one; frames count from 1
    struct FrameCaptureConfig {
        u64 frame = 0;
        Str path;
    };

    struct RuntimeConfig {
        Str name = "AnonymousApp";
        Str version = "v0.0.1";
        Str identifier = "AnonymousIdentifier";

        WindowConfig window;
        BenchmarkConfig benchmark;

        // argv only: --hold starts the loop held before frame 1. Debug
        // builds only, since only the command port can release it
        bool hold = false;
        // argv only: --capture-frame <frame> <path>, once per frame
        std::vector<FrameCaptureConfig> captures;
        // argv only: --exit-after-frame <frame> ends the loop once that frame
        // is presented, so a run's length is counted, not timed
        std::optional<u64> exitAfterFrame;
        // argv only: --port-input takes the keyboard and mouse from the
        // command port's push_input alone; the devices are not heard
        bool portInput = false;
        // argv only: --project <folder> stands in for the project root found
        // above the executable
        std::optional<std::filesystem::path> project;
    };

    template<>
    struct DomTraits<RuntimeConfig> {
        // refuses any document whose metadata.type is not "app"
        static RuntimeConfig from(const DOM::Value&, const DocMetadata&);
    };

    // The arguments after the program name, as Usage lists them; a --config
    // app document replaces the sample's benchmark and window.vsync.
    void applyCommandLine(RuntimeConfig&, std::span<char* const> args);
}
